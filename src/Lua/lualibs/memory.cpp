#include "../lualibs.hpp"
#include "../script.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedObject.h>
#include <windows.h>

using namespace luabridge;

namespace {
    struct cpustate {
        int edi, esi, ebp, esp,
            ebx, edx, ecx, eax;
    };

    class FuncCall : public RefCountedObject {
    public:
        lua_State* const L;
        size_t addrOrIndex;
        const size_t argc;
        bool isThisCall, isVirtual;

        inline FuncCall(lua_State* L, size_t addrOrIndex, size_t argc, bool thisCall, bool isVirtual)
            : L(L), addrOrIndex(addrOrIndex), argc(argc), isThisCall(thisCall), isVirtual(isVirtual) {}

        int luacall(lua_State* L) {
            size_t c = argc + (isThisCall ? 1 : 0);
            if (lua_gettop(L) < c) return luaL_error(L, "FuncCall called with invalid number of arguments.");
            int* argv = new int[c ? c : 1]; // alloc at least one integer
            for (int i = 2; i <= c+1; ++i) argv[i-2] = luaL_checkinteger(L, i);

            const auto _address = isVirtual ? (*(size_t**)argv[0])[addrOrIndex] : addrOrIndex;
            const auto _argc = argc;
            size_t _ret;
            __asm {
                mov esi, esp;
                mov edi, argv;
                mov ebx, _argc;
                xor edx, edx;           // init registers
            argloop:
                cmp edx, ebx;
                jge endloop;            // for (i = 0; i < argc; ++i)
                mov ecx, c;
                sub ecx, edx;
                mov ecx, [edi+ecx*4-4]; // ecx = argv[c-i-1]
                push ecx;               // push arg
                inc edx;
                jmp argloop;
            endloop:
                mov ecx, [edi];         // first argument as ecx
                mov eax, _address;
                call eax;               // ret = call(...)
                mov esp, esi;
                mov _ret, eax;
            }

            delete[] argv;
            lua_pushinteger(L, _ret);
            return 1;
        }
    };

#pragma optimize( "", off )
    class Callback : public RefCountedObject {
    public:
        lua_State* const L;
        const int ref;
        const size_t argc;

        bool enabled = true;
        inline Callback(lua_State* L, int ref, size_t argc) : L(L), ref(ref), argc(argc) {}

        bool call(cpustate& state, bool isRegisterWritable = false) {
            if (!enabled) return false;

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (isRegisterWritable) Stack<cpustate&>::push(L, state);
            else Stack<const cpustate&>::push(L, state);
            int i = 0; while(i < argc) {
                lua_pushinteger(L, ((int*)state.esp)[++i]);
            }

            auto ret = lua_pcall(L, argc+1, 1, 0);
            if (ret) {
                Logger::Error(lua_tostring(L, -1));
                lua_pop(L, 1);
                return false;
            } else if (!lua_isnil(L, -1)) {
                state.eax = lua_tointeger(L, -1);
                lua_pop(L, 1);
                return true;
            } else {
                lua_pop(L, 1);
                return false;
            }
        }

        int luacall(lua_State* L2) {
            if (!enabled) return 0;

            size_t c = argc < lua_gettop(L2) ? argc : lua_gettop(L2);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            lua_pushnil(L);
            for (int i = 0; i < c; ++i) lua_pushinteger(L, lua_tointeger(L2, i));

            auto ret = lua_pcall(L, c+1, 1, 0);
            if (ret) {
                Logger::Error(lua_tostring(L, -1));
                lua_pop(L, 1);
                return 0;
            }

            int retValue = lua_tointeger(L, 1);
            lua_pop(L, 1);
            lua_pushinteger(L2, retValue);
            return 1;
        }
    };

    struct BaseHook {
        std::list<RefCountedObjectPtr<Callback>> callbacks;
        virtual inline bool isRegisterWritable() { return false; }

        int __cdecl listener(cpustate state) {
            for (auto& cb : callbacks) if (cb->call(state, isRegisterWritable())) return 1;
            return 0;
        }

        inline BaseHook(void* shim, size_t shimsize) {
            DWORD oldProt; VirtualProtect(shim, shimsize, PAGE_EXECUTE_READWRITE, &oldProt);
        }
    };
#pragma optimize( "", on )

    struct CallHook : public BaseHook {
    protected:
        const unsigned char shim[23] = {
            0x60,                           // pushad
            0x68, 0x00, 0x00, 0x00, 0x00,   // push this
            0xE8, 0x00, 0x00, 0x00, 0x00,   // call listener
            0x59,                           // pop this
            0x85, 0xC0,                     // test eax, eax
            0x61,                           // popad
            0x75, 0x05,                     // jne skip:
            0xE9, 0x00, 0x00, 0x00, 0x00,   // jmp original_addr
            // skip:
            0xC3,                           // ret
        };

    public:
        inline CallHook(DWORD addr) : BaseHook((void*)shim, sizeof(shim)) {
            DWORD oldProt;
            VirtualProtect((LPVOID)addr, 5, PAGE_EXECUTE_READWRITE, &oldProt);
            auto original = SokuLib::TamperNearJmpOpr(addr, shim);
            VirtualProtect((LPVOID)addr, 5, oldProt, &oldProt);

            *(int*)&shim[2] = (int)this;
            SokuLib::TamperNearJmpOpr((DWORD)&shim[6], &BaseHook::listener);
            SokuLib::TamperNearJmpOpr((DWORD)&shim[17], original);
        }
    };

    struct VTableHook : public BaseHook {
    protected:
        const unsigned char shim[23] = {
            0x60,                           // pushad
            0x68, 0x00, 0x00, 0x00, 0x00,   // push this
            0xE8, 0x00, 0x00, 0x00, 0x00,   // call listener
            0x59,                           // pop this
            0x85, 0xC0,                     // test eax, eax
            0x61,                           // popad
            0x75, 0x05,                     // jne skip:
            0xE9, 0x00, 0x00, 0x00, 0x00,   // jmp original_addr
            // skip:
            0xC3,                           // ret
        };

    public:
        inline VTableHook(DWORD addr) : BaseHook((void*)shim, sizeof(shim)) {
            DWORD oldProt;
            VirtualProtect((LPVOID)addr, 4, PAGE_EXECUTE_READWRITE, &oldProt);
            auto original = SokuLib::TamperDword(addr, shim);
            VirtualProtect((LPVOID)addr, 4, oldProt, &oldProt);

            *(int*)&shim[2] = (int)this;
            SokuLib::TamperNearJmpOpr((DWORD)&shim[6], &BaseHook::listener);
            SokuLib::TamperNearJmpOpr((DWORD)&shim[17], original);
        }
    };

    struct TrampHook : public BaseHook {
    protected:
        const unsigned char shim[34] = {
            0x60,                           // pushad
            0x68, 0x00, 0x00, 0x00, 0x00,   // push this
            0xE8, 0x00, 0x00, 0x00, 0x00,   // call listener
            0x59,                           // pop this
            0x61,                           // popad
            // extra data [16+5]
            0x90, 0x90, 0x90, 0x90,
            0x90, 0x90, 0x90, 0x90,
            0x90, 0x90, 0x90, 0x90,
            0x90, 0x90, 0x90, 0x90,
            0x90, 0x90, 0x90, 0x90, 0x90,
        };
        virtual inline bool isRegisterWritable() { return true; }

    public:
        inline TrampHook(DWORD addr, size_t opsize) : BaseHook((void*)shim, sizeof(shim)) {
            if (opsize < 5 || opsize > 16) throw std::runtime_error("Error: Operator size limit (5, 16).");

            DWORD oldProt;
            VirtualProtect((LPVOID)addr, opsize, PAGE_EXECUTE_READWRITE, &oldProt);
            memcpy((void*)&shim[13], (void*)addr, opsize);
            SokuLib::TamperNearJmp(addr, shim);
            memset((void*)(addr + 5), 0x90, opsize - 5);
            VirtualProtect((LPVOID)addr, opsize, oldProt, &oldProt);

            *(int*)&shim[2] = (int)this;
            SokuLib::TamperNearJmpOpr((DWORD)&shim[6], &BaseHook::listener);
            SokuLib::TamperNearJmp((DWORD)&shim[13 + opsize], addr + opsize);
        }
    };

    std::unordered_map<size_t, BaseHook*> hookedAddr;
    std::unordered_map<std::string, RefCountedObjectPtr<Callback>> IPCmap;
}

void ShadyLua::RemoveMemoryEvents(LuaScript* script) {
    for (auto hook : hookedAddr) {
        for (auto iter = hook.second->callbacks.begin(); iter != hook.second->callbacks.end();) {
            if (iter->getObject()->L == script->L) iter = hook.second->callbacks.erase(iter);
            else ++iter;
        }
    }

    for (auto iter = IPCmap.begin(); iter != IPCmap.end();) {
        if (iter->second->L == script->L) iter = IPCmap.erase(iter);
        else ++iter;
    }
}

/** Read from memory into a string */
static std::string memory_readbytes(int address, int size) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    std::string value((char*)address, size);
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, dwOldProtect, &dwOldProtect);
    return value;
}

/** Read a double from memory */
static double memory_readdouble(int address) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    double value = *(double*)address;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, dwOldProtect, &dwOldProtect);
    return value;
}

/** Read a float from memory */
static float memory_readfloat(int address) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    float value = *(float*)address;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, dwOldProtect, &dwOldProtect);
    return value;
}

/** Read a int from memory */
static int memory_readint(int address) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    int value = *(int*)address;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, dwOldProtect, &dwOldProtect);
    return value;
}

/** Read a short from memory */
static int memory_readshort(int address) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 2, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    short value = *(short*)address;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 2, dwOldProtect, &dwOldProtect);
    return value;
}

/** Writes a string into memory */
static void memory_writebytes(int address, std::string value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), value.size(), PAGE_EXECUTE_READWRITE, &dwOldProtect);
    memcpy((void*)address, value.c_str(), value.size());
    VirtualProtect(reinterpret_cast<LPVOID>(address), value.size(), dwOldProtect, &dwOldProtect);
}

/** Writes a double into memory */
static void memory_writedouble(int address, double value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(double*)address = value;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, dwOldProtect, &dwOldProtect);
}

/** Writes a float into memory */
static void memory_writefloat(int address, float value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(float*)address = value;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, dwOldProtect, &dwOldProtect);
}

/** Writes a int into memory */
static void memory_writeint(int address, int value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(int*)address = value;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, dwOldProtect, &dwOldProtect);
}

/** Writes a int into memory */
static void memory_writeshort(int address, short value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 2, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(short*)address = value;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 2, dwOldProtect, &dwOldProtect);
}

static int memory_createcallback(lua_State* L) {
    int argc = luaL_checkinteger(L, 1);

    if(!lua_isfunction(L, 2)) return luaL_error(L, "createcallback must receive a function.");
    lua_pushvalue(L, 2);
    RefCountedObjectPtr<Callback> cb(new Callback(L, luaL_ref(L, LUA_REGISTRYINDEX), argc));

    luabridge::push(L, cb);
    return 1;
}

static int memory_createfunccall(lua_State* L) {
    if (lua_gettop(L) < 3) return luaL_error(L, "invalid number of arguments");
    int addr = luaL_checkinteger(L, 1);
    int argc = luaL_checkinteger(L, 2);
    bool thisCall = lua_toboolean(L, 3);

    RefCountedObjectPtr<FuncCall> cb(new FuncCall(L, addr, argc, thisCall, false));

    luabridge::push(L, cb);
    return 1;
}

static int memory_createvirtualcall(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    int argc = luaL_checkinteger(L, 2);

    RefCountedObjectPtr<FuncCall> cb(new FuncCall(L, index, argc, true, true));

    luabridge::push(L, cb);
    return 1;
}

static bool memory_hookcall(size_t addr, RefCountedObjectPtr<Callback> cb) {
    auto result = hookedAddr.insert(std::make_pair(addr, nullptr));
    if (result.second) result.first->second = new CallHook(addr);
    result.first->second->callbacks.push_back(cb);
    return result.second;
}

static bool memory_hookvtable(size_t addr, RefCountedObjectPtr<Callback> cb) {
    auto result = hookedAddr.insert(std::make_pair(addr, nullptr));
    if (result.second) result.first->second = new VTableHook(addr);
    result.first->second->callbacks.push_back(cb);
    return result.second;
}

static bool memory_hooktramp(size_t addr, size_t opsize, RefCountedObjectPtr<Callback> cb) {
    auto result = hookedAddr.insert(std::make_pair(addr, nullptr));
    if (result.second) result.first->second = new TrampHook(addr, opsize);
    result.first->second->callbacks.push_back(cb);
    return result.second;
}

static void memory_setIPC(const std::string& name, RefCountedObjectPtr<Callback> cb) { IPCmap[name] = cb; }
static RefCountedObjectPtr<Callback> memory_getIPC(const std::string& name) {
    auto iter = IPCmap.find(name);
    if (iter == IPCmap.end()) return nullptr;
    return iter->second;
}

void ShadyLua::LualibMemory(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("memory")
            .addFunction("readbytes", memory_readbytes)
            .addFunction("readdouble", memory_readdouble)
            .addFunction("readfloat", memory_readfloat)
            .addFunction("readint", memory_readint)
            .addFunction("readshort", memory_readshort)
            .addFunction("writebytes", memory_writebytes)
            .addFunction("writedouble", memory_writedouble)
            .addFunction("writefloat", memory_writefloat)
            .addFunction("writeint", memory_writeint)
            .addFunction("writeshort", memory_writeshort)

            .beginClass<cpustate>("CPUState")
                .addProperty("eax", &cpustate::eax)
                .addProperty("ecx", &cpustate::ecx)
                .addProperty("edx", &cpustate::edx)
                .addProperty("ebx", &cpustate::ebx)
                .addProperty("esp", &cpustate::esp)
                .addProperty("ebp", &cpustate::ebp)
                .addProperty("esi", &cpustate::esi)
                .addProperty("edi", &cpustate::edi)
            .endClass()

            .beginClass<Callback>("Callback")
                .addProperty("enabled", &Callback::enabled)
                .addFunction("__call", &Callback::luacall)
            .endClass()
            .beginClass<FuncCall>("FuncCall")
                .addFunction("__call", &FuncCall::luacall)
            .endClass()
            .addCFunction("createcallback", memory_createcallback)
            .addCFunction("createfunccall", memory_createfunccall)
            .addCFunction("createvirtualcall", memory_createvirtualcall)
            .addFunction("hookcall", memory_hookcall)
            .addFunction("hookvtable", memory_hookvtable)
            .addFunction("hooktramp", memory_hooktramp)

            .addFunction("setIPC", memory_setIPC)
            .addFunction("getIPC", memory_getIPC)
            .addFunction("new", reinterpret_cast<size_t (*const)(size_t)>(SokuLib::NewFct))
            .addFunction("delete", reinterpret_cast<void (*const)(size_t)>(SokuLib::DeleteFct))
        .endNamespace()
    ;
}