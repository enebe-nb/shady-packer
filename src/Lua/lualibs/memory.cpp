#include "../lualibs.hpp"
#include "../script.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedObject.h>
#include <windows.h>

using namespace luabridge;

namespace {
    template <int value> static inline int* enumMap() {
        static const int valueHolder = value; return (int*)&valueHolder;
    }

    struct cpustate {
        int edi, esi, ebp, esp,
            ebx, edx, ecx, eax;
    };

    class FuncCall : public RefCountedObject {
    public:
        //lua_State* const L;
        size_t addrOrIndex;
        const size_t argc;
        bool isVirtual;
        enum CALLCONVS : uint8_t {_CDECL=0, _STDCALL=0, _THISCALL=1, _FASTCALL=2} callConv;
        inline FuncCall(lua_State* L, size_t addrOrIndex, size_t argc, uint8_t callconv, bool isVirtual)
            : addrOrIndex(addrOrIndex), argc(argc), callConv((CALLCONVS)callconv), isVirtual(isVirtual) {}

        int luacall(lua_State* L) {
            size_t c = argc + 2;// + ecx & edx slot

            if (lua_gettop(L) - 1 < argc + callConv) {
                return luaL_error(L,
                    "FuncCall: expected %d arguments, got %d.",
                    argc + callConv, lua_gettop(L) - 1);
            }
			int* argv = new int[c] {0};//ecx, edx, sarg1, sarg2..
            for (int i = 0, k=2; i < c; ++i) {
                if (i>=2 || i<callConv) {
                    argv[i] = luaL_checkinteger(L, k++);
                }
            }

            if (argv[0]==0 && callConv==_THISCALL) {
                delete[] argv;
                return luaL_error(L, "Thiscall function requires non-null this pointer.");
            }
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
                mov edx, [edi+4]
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
        using t_copyfn = void(lua_State* Ls, int index, lua_State* Lt);
        static t_copyfn* get_userdata_copier(lua_State* Ls, int index) {
            int result = lua_getmetatable(Ls, index);
            if (result == 0) return nullptr;
            if (!lua_istable(Ls, -1)) {
                lua_pop(Ls, 1);
                return nullptr;
            }
            auto buffer = ShadyLua::ScriptMap[Ls]->IPCCopierBuffer;
			auto iter = buffer.find(lua_topointer(Ls, -1));
            lua_pop(Ls, 1);
            if (iter != buffer.end()) {
                return iter->second;
			} return nullptr;
        }
        static int interstate_copy(lua_State* Ls, int index, lua_State* Lt) {
            index = lua_absindex(Ls, index);
            auto t = lua_type(Ls, index);
            switch (t) {
            case LUA_TNIL:
                lua_pushnil(Lt);
                break;
            case LUA_TBOOLEAN:
                lua_pushboolean(Lt, lua_toboolean(Ls, index));
                break;
            case LUA_TNUMBER:
                if (lua_isinteger(Ls, index)) {
                    lua_pushinteger(Lt, lua_tointeger(Ls, index));
                } else {
                    lua_pushnumber(Lt, lua_tonumber(Ls, index));
                }
                break;
            case LUA_TSTRING: {
                size_t len; const char* str = lua_tolstring(Ls, index, &len);
                lua_pushlstring(Lt, str, len);
                break;
            }
            case LUA_TTABLE: {
                int o = lua_gettop(Lt);
                lua_newtable(Lt);
                lua_pushnil(Ls);
                while (lua_next(Ls, index) != 0) {
                    int ret = interstate_copy(Ls, -2, Lt); // copy key
                    if (ret) {
                        lua_pop(Ls, 2);
                        lua_settop(Lt, o);
                        return ret;
                    }
                    ret = interstate_copy(Ls, -1, Lt); // copy value; didn't resolve case `t.self=t`
                    if (ret) {
                        lua_pop(Ls, 2);
                        lua_settop(Lt, o);
                        return ret;
                    }
                    lua_settable(Lt, -3);
                    lua_pop(Ls, 1);
                }
                break;
            }
            case LUA_TLIGHTUSERDATA:
                lua_pushlightuserdata(Lt, lua_touserdata(Ls, index));
                break;
            case LUA_TUSERDATA: {
                auto fn = get_userdata_copier(Ls, index);
                if (fn == nullptr) return 2;
                fn(Ls, index, Lt);
                break;
            }
            default:
                return 1;
            }
            return 0;
        }
    public:
        lua_State* const L;
        const int ref;
        const size_t argc;

        bool enabled = true;
        inline Callback(lua_State* L, int ref, size_t argc) : L(L), ref(ref), argc(argc) {}

        bool call(cpustate& state) {
            if (!enabled) return false;
            std::lock_guard scriptGuard(ShadyLua::ScriptMap[L]->mutex);

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            Stack<cpustate&>::push(L, state);
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
            std::lock_guard scriptGuard(ShadyLua::ScriptMap[L]->mutex);
            size_t c = lua_gettop(L2);
            c = (c >= 1) ? (c - 1) : 0;//exclude Callback self
            c = min(c, argc);
            
            auto o = lua_gettop(L);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
			lua_pushnil(L);//for fake cpu state parameter
            if (L2 != L) {//real IPC
                //for (int i = 0; i < c; ++i) lua_pushinteger(L, lua_tointeger(L2, i+2));
                for (int k = 2; k < c + 2; ++k) {
                    //auto arg = luabridge::LuaRef::fromStack(L2, k); arg.push(L);
                    if (interstate_copy(L2, k, L)) {
                        lua_settop(L, o);
                        return luaL_error(L2, "IPC: parameter contains unsupported argument type.");
                    }
                }

                auto ret = lua_pcall(L, c+1, LUA_MULTRET, 0);
                if (ret) {
                    Logger::Error(lua_tostring(L, -1));
                    lua_settop(L, o);
                    return 0;
                }
                int nret = lua_gettop(L) - o;
                for (int k = o+1; k < nret+o+1; k++) {
                    //lua_pushinteger(L2, lua_tointeger(L, k));
                    //auto rets = luabridge::LuaRef::fromStack(L, k); rets.push(L2);
                    if (interstate_copy(L, k, L2)) {
                        lua_settop(L, o);
                        return luaL_error(L2, "IPC: return value contains unsupported argument type.");
                    }
                }
                lua_settop(L, o);
                return nret;
            } else {//call from the same state (for test?)
                for (int i = 0; i < c; ++i) lua_pushvalue(L, i+2);
                auto ret = lua_pcall(L, c + 1, LUA_MULTRET, 0);
                if (ret) {
                    Logger::Error(lua_tostring(L, -1));
                    lua_settop(L, o);
                    return 0;
                }
                return lua_gettop(L)-o;
            }
        }
    };

    struct BaseHook {
        std::list<RefCountedObjectPtr<Callback>> callbacks;

        int __cdecl listener(cpustate state) {
            for (auto& cb : callbacks) if (cb->call(state)) return 1;
            return 0;
        }

        inline BaseHook(void* shim, size_t shimsize) {
            DWORD oldProt; VirtualProtect(shim, shimsize, PAGE_EXECUTE_READWRITE, &oldProt);
        }
    };
#pragma optimize( "", on )

    struct CallHook : public BaseHook {
    protected:
        const unsigned char shim[25] = {
            0x60,                           // pushad
            0x68, 0x00, 0x00, 0x00, 0x00,   // push this
            0xE8, 0x00, 0x00, 0x00, 0x00,   // call listener
            0x59,                           // pop this
            0x85, 0xC0,                     // test eax, eax
            0x61,                           // popad
            0x75, 0x05,                     // jne skip:
            0xE9, 0x00, 0x00, 0x00, 0x00,   // jmp original_addr
            // skip:
            0xC3, 0x90, 0x90,               // ret ?
        };

    public:
        inline CallHook(DWORD addr, unsigned short argv = 0) : BaseHook((void*)shim, sizeof(shim)) {
            DWORD oldProt;
            VirtualProtect((LPVOID)addr, 5, PAGE_EXECUTE_READWRITE, &oldProt);
            auto original = SokuLib::TamperNearJmpOpr(addr, shim);
            VirtualProtect((LPVOID)addr, 5, oldProt, &oldProt);

            *(int*)&shim[2] = (int)this;
            SokuLib::TamperNearJmpOpr((DWORD)&shim[6], &BaseHook::listener);
            SokuLib::TamperNearJmpOpr((DWORD)&shim[17], original);
            if (argv > 0) {
                *(unsigned char*)&shim[22] = (unsigned char)0xC2;
                *(unsigned short*)&shim[23] = (unsigned short)argv*4;
            }
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
    uint8_t callconv = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : lua_tointeger(L, 3);
    if (callconv == FuncCall::CALLCONVS::_FASTCALL) {
        argc -= 2;//refine ecx,edx
    }

    RefCountedObjectPtr<FuncCall> cb(new FuncCall(L, addr, argc, callconv, false));

    luabridge::push(L, cb);
    return 1;
}

static int memory_createvirtualcall(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    int argc = luaL_checkinteger(L, 2);

    RefCountedObjectPtr<FuncCall> cb(new FuncCall(L, index, argc, FuncCall::CALLCONVS::_THISCALL, true));

    luabridge::push(L, cb);
    return 1;
}

//static bool memory_hookcall(size_t addr, RefCountedObjectPtr<Callback> cb, unsigned short argv = 0) {
static int memory_hookcall(lua_State* L) {
    size_t addr = luaL_checkinteger(L, 1);
    RefCountedObjectPtr<Callback> cb(Stack<Callback*>::get(L, 2));
    unsigned short argv = luaL_optinteger(L, 3, 0);

    auto result = hookedAddr.insert(std::make_pair(addr, nullptr));
    if (result.second) result.first->second = new CallHook(addr, argv);
    result.first->second->callbacks.push_back(cb);
    lua_pushboolean(L, result.second);
    return 1;
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

void ShadyLua::_insert_map_helper(lua_State* L, const void* pmt, void(*pf)(lua_State*, int, lua_State*)) {
    ShadyLua::ScriptMap[L]->IPCCopierBuffer[pmt] = pf;
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
		        .addStaticProperty("CDECL", enumMap<FuncCall::CALLCONVS::_CDECL>(), false)
                .addStaticProperty("STDCALL", enumMap<FuncCall::CALLCONVS::_STDCALL>(), false)
                .addStaticProperty("THISCALL", enumMap<FuncCall::CALLCONVS::_THISCALL>(), false)
                .addStaticProperty("FASTCALL", enumMap<FuncCall::CALLCONVS::_FASTCALL>(), false)
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
    RegisterIPCUserdata<cpustate, false>(L);
    RegisterIPCUserdata<Callback>(L);
    RegisterIPCUserdata<FuncCall>(L);
}