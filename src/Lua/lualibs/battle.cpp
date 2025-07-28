#include "../lualibs.hpp"
#include "../script.hpp"
#include "gui.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/Vector.h>

using namespace luabridge;

#define MEMBER_ADDRESS(u,s,m) SokuLib::union_cast<u s::*>(&reinterpret_cast<char const volatile&>(((s*)0)->m))

int ShadyLua::CustomDataProxy::__index(lua_State* L) {
    int index = luaL_checkinteger(L, 2);
    if (index <= 0) return luaL_argerror(L, 2, "index out of bounds.");
    Stack<float>::push(L, ((float*)addr)[index - 1]);
    return 1;
}
int ShadyLua::CustomDataProxy::__newindex(lua_State* L) {
    int index = luaL_checkinteger(L, 2);
    if (index <= 0) return luaL_argerror(L, 2, "index out of bounds.");
    ((float*)addr)[index - 1] = Stack<float>::get(L, 3);
    return 0;
}

namespace {
    struct playerDataHash {
        inline std::size_t operator()(const std::pair<lua_State*, SokuLib::v2::GameObjectBase*>& key) const {
            return ((size_t)key.first) ^ ((size_t)key.second);
        }
    };
    std::unordered_map<std::pair<lua_State*, SokuLib::v2::Player*>, LuaRef, playerDataHash> playerData;
    static inline LuaRef& playerDataInsertOrGet(lua_State* L, SokuLib::v2::Player* player) {
        return playerData.insert({std::pair(L, player), newTable(L)}).first->second;
    }

    struct HookData {
        int updateHandler = LUA_REFNIL;
        int initSeqHandler = LUA_REFNIL;
        // int update2Handler = LUA_REFNIL;
        int initHandler = LUA_REFNIL;
        ShadyLua::LuaScript* script;

        inline HookData(lua_State* L) : script(ShadyLua::ScriptMap[L]) {}
    };

    std::list<std::list<HookData>*> allListeners;

    template <class T>
    static void __declspec(naked) customUpdateHook() {
        __asm {
            pop eax;                            // remove return address
            sub esp, 108;
            fsave [esp];                        // save FPU to stack
            mov ecx, esi;
            call ObjectHook<T>::replUpdate;     // call our logic
            cmp al, 0;
            jne skipUpdate;                     // if return is true, skip default logic
            mov eax, ObjectHook<T>::origUpdate; // return to default logic
            jmp retLogic;
        skipUpdate:
            mov eax, ObjectHook<T>::customUpdateRetAddress;  // return to end of function
        retLogic:
            frstor [esp];
            add esp, 108;                       // restore FPU from stack
            pop ecx;
            pop edx;                            // restore ecx, edx (saved from hook call)
            push eax;                           // pushes the return address we want
            movsx eax, word ptr [esi+0x13c];    // redo replaced code
            ret;
        }
    }

    template <class T>
    struct ObjectHook : ShadyLua::Hook {
        static_assert(std::is_base_of<SokuLib::v2::GameObjectBase, T>::value, "Object not derived from GameObjectBase");
        using typeFn = bool (__fastcall *)(T*);
        static inline typeFn origUpdate, origInitSeq, origInit;
        static inline std::list<HookData> listeners;
        static const DWORD customUpdateAddress, customUpdateRetAddress;

        static void __fastcall simpleUpdateHook(T* object) {
            if (!replUpdate(object)) origUpdate(object);
        }

        static bool __fastcall replUpdate(T* object) {
            for (auto& data : listeners) {
                if (data.updateHandler == LUA_REFNIL) continue;
                auto L = data.script->L;

                lua_rawgeti(L, LUA_REGISTRYINDEX, data.updateHandler);
                if constexpr(std::is_base_of<SokuLib::v2::Player, T>::value) {
                    luabridge::push(L, (SokuLib::v2::Player*)object);
                    lua_pushinteger(L, object->frameState.actionId);
                    luabridge::push(L, playerDataInsertOrGet(L, (SokuLib::v2::Player*) object));
                } else {
                    luabridge::push(L, (SokuLib::v2::GameObject*)object);
                    lua_pushinteger(L, object->frameState.actionId);
                    luabridge::push(L, playerDataInsertOrGet(L, object->gameData.owner));
                }

                if (lua_pcall(L, 3, 1, 0)) {
                    Logger::Error(lua_tostring(L, -1));
                    lua_pop(L, 1);
                } else if (!lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    return true;
                } else lua_pop(L, 1);
            } return false;
        }

        static bool __fastcall replInitAction(T* object) {
            bool ret, skip = false;

            for (auto& data : listeners) {
                if (data.initSeqHandler == LUA_REFNIL) continue;
                auto L = data.script->L;

                lua_rawgeti(L, LUA_REGISTRYINDEX, data.initSeqHandler);
                if constexpr(std::is_base_of<SokuLib::v2::Player, T>::value) {
                    luabridge::push(L, (SokuLib::v2::Player*)object);
                    lua_pushinteger(L, object->frameState.actionId);
                    luabridge::push(L, playerDataInsertOrGet(L, (SokuLib::v2::Player*) object));
                } else {
                    luabridge::push(L, (SokuLib::v2::GameObject*)object);
                    lua_pushinteger(L, object->frameState.actionId);
                    luabridge::push(L, playerDataInsertOrGet(L, object->gameData.owner));
                }

                if (lua_pcall(L, 3, 1, 0)) {
                    Logger::Error(lua_tostring(L, -1));
                    lua_pop(L, 1);
                } else if (!lua_isnil(L, -1)) {
                    ret = lua_toboolean(L, -1);
                    skip = true;
                    lua_pop(L, 1);
                    break;
                } else lua_pop(L, 1);
            }

            if (!skip) return origInitSeq(object);
            return ret;
        }

        static bool __fastcall replInit(T* object) {
            bool ret = origInit(object);

            for (auto& data : listeners) {
                if (data.initHandler == LUA_REFNIL) continue;
                auto L = data.script->L;

                lua_rawgeti(L, LUA_REGISTRYINDEX, data.initHandler);
                if constexpr(std::is_base_of<SokuLib::v2::Player, T>::value) {
                    luabridge::push(L, (SokuLib::v2::Player*)object);
                    luabridge::push(L, playerDataInsertOrGet(L, (SokuLib::v2::Player*) object));
                } else {
                    luabridge::push(L, (SokuLib::v2::GameObject*)object);
                    luabridge::push(L, playerDataInsertOrGet(L, object->gameData.owner));
                }

                if (lua_pcall(L, 2, 1, 0)) {
                    Logger::Error(lua_tostring(L, -1));
                } lua_pop(L, 1);
            }

            return ret;
        }

        inline ObjectHook() {
            const DWORD ADDR = SokuLib::_vtable_info<T>::baseAddr;
            DWORD prot; VirtualProtect((LPVOID)ADDR, 19*4, PAGE_EXECUTE_WRITECOPY, &prot);

            if constexpr(std::is_base_of<SokuLib::v2::Player, T>::value) {
                DWORD prot2; VirtualProtect((LPVOID)customUpdateAddress, 7, PAGE_EXECUTE_WRITECOPY, &prot2);
                *(short*)(customUpdateAddress) = 0x5152; // push edx; push ecx;
                SokuLib::TamperNearCall(customUpdateAddress + 2, customUpdateHook<T>);
                origUpdate = reinterpret_cast<typeFn>(customUpdateAddress + 7);
                VirtualProtect((LPVOID)customUpdateAddress, 7, prot2, &prot2);
            } else origUpdate = reinterpret_cast<typeFn>(SokuLib::TamperDword(SokuLib::GetVirtualTableOf(&T::update), simpleUpdateHook));

            origInitSeq = SokuLib::TamperDword(SokuLib::GetVirtualTableOf(&T::initializeAction), replInitAction);
            if constexpr(std::is_base_of<SokuLib::v2::Player, T>::value)
                origInit = SokuLib::TamperDword(SokuLib::GetVirtualTableOf(&T::initialize), replInit);

            VirtualProtect((LPVOID)ADDR, 19*4, prot, &prot);
        }

        virtual ~ObjectHook() {
            const DWORD ADDR = SokuLib::_vtable_info<T>::baseAddr;
            DWORD prot; VirtualProtect((LPVOID)ADDR, 19*4, PAGE_EXECUTE_WRITECOPY, &prot);
            SokuLib::TamperDword(SokuLib::GetVirtualTableOf(&T::update), origUpdate);
            SokuLib::TamperDword(SokuLib::GetVirtualTableOf(&T::initializeAction), origInitSeq);
            if constexpr(std::is_base_of<SokuLib::v2::Player, T>::value)
                SokuLib::TamperDword(SokuLib::GetVirtualTableOf(&T::initialize), origInit);
            VirtualProtect((LPVOID)ADDR, 19*4, prot, &prot);
        }
    };

    const DWORD ObjectHook<SokuLib::v2::PlayerReimu>::customUpdateAddress           = 0x490ba7;
    const DWORD ObjectHook<SokuLib::v2::PlayerReimu>::customUpdateRetAddress        = 0x4a3dca;
    const DWORD ObjectHook<SokuLib::v2::PlayerMarisa>::customUpdateAddress          = 0x4bdd9c;
    const DWORD ObjectHook<SokuLib::v2::PlayerMarisa>::customUpdateRetAddress       = 0x4d0213;
    const DWORD ObjectHook<SokuLib::v2::PlayerSakuya>::customUpdateAddress          = 0x4e7a4b;
    const DWORD ObjectHook<SokuLib::v2::PlayerSakuya>::customUpdateRetAddress       = 0x4ed913;
    const DWORD ObjectHook<SokuLib::v2::PlayerAlice>::customUpdateAddress           = 0x510582;
    const DWORD ObjectHook<SokuLib::v2::PlayerAlice>::customUpdateRetAddress        = 0x51e16a;
    const DWORD ObjectHook<SokuLib::v2::PlayerPatchouli>::customUpdateAddress       = 0x541eee;
    const DWORD ObjectHook<SokuLib::v2::PlayerPatchouli>::customUpdateRetAddress    = 0x553c5c;
    const DWORD ObjectHook<SokuLib::v2::PlayerYoumu>::customUpdateAddress           = 0x56ccc4;
    const DWORD ObjectHook<SokuLib::v2::PlayerYoumu>::customUpdateRetAddress        = 0x578efd;
    const DWORD ObjectHook<SokuLib::v2::PlayerRemilia>::customUpdateAddress         = 0x587bab;
    const DWORD ObjectHook<SokuLib::v2::PlayerRemilia>::customUpdateRetAddress      = 0x599fb0;
    const DWORD ObjectHook<SokuLib::v2::PlayerYuyuko>::customUpdateAddress          = 0x5a59c5;
    const DWORD ObjectHook<SokuLib::v2::PlayerYuyuko>::customUpdateRetAddress       = 0x5a8c97;
    const DWORD ObjectHook<SokuLib::v2::PlayerYukari>::customUpdateAddress          = 0x5cac79;
    const DWORD ObjectHook<SokuLib::v2::PlayerYukari>::customUpdateRetAddress       = 0x5d1781;
    const DWORD ObjectHook<SokuLib::v2::PlayerSuika>::customUpdateAddress           = 0x5ee7ad;
    const DWORD ObjectHook<SokuLib::v2::PlayerSuika>::customUpdateRetAddress        = 0x5f1713;
    const DWORD ObjectHook<SokuLib::v2::PlayerUdonge>::customUpdateAddress          = 0x61e018;
    const DWORD ObjectHook<SokuLib::v2::PlayerUdonge>::customUpdateRetAddress       = 0x6311fa;
    const DWORD ObjectHook<SokuLib::v2::PlayerAya>::customUpdateAddress             = 0x6653a8;
    const DWORD ObjectHook<SokuLib::v2::PlayerAya>::customUpdateRetAddress          = 0x66b832;
    const DWORD ObjectHook<SokuLib::v2::PlayerKomachi>::customUpdateAddress         = 0x644aed;
    const DWORD ObjectHook<SokuLib::v2::PlayerKomachi>::customUpdateRetAddress      = 0x6561f4;
    const DWORD ObjectHook<SokuLib::v2::PlayerIku>::customUpdateAddress             = 0x687c34;
    const DWORD ObjectHook<SokuLib::v2::PlayerIku>::customUpdateRetAddress          = 0x699e45;
    const DWORD ObjectHook<SokuLib::v2::PlayerTenshi>::customUpdateAddress          = 0x6ab70b;
    const DWORD ObjectHook<SokuLib::v2::PlayerTenshi>::customUpdateRetAddress       = 0x6ba7ff;
    const DWORD ObjectHook<SokuLib::v2::PlayerSanae>::customUpdateAddress           = 0x73cadb;
    const DWORD ObjectHook<SokuLib::v2::PlayerSanae>::customUpdateRetAddress        = 0x74717b;
    const DWORD ObjectHook<SokuLib::v2::PlayerChirno>::customUpdateAddress          = 0x6e36c5;
    const DWORD ObjectHook<SokuLib::v2::PlayerChirno>::customUpdateRetAddress       = 0x6e8b7d;
    const DWORD ObjectHook<SokuLib::v2::PlayerMeirin>::customUpdateAddress          = 0x71ac6c;
    const DWORD ObjectHook<SokuLib::v2::PlayerMeirin>::customUpdateRetAddress       = 0x726e28;
    const DWORD ObjectHook<SokuLib::v2::PlayerUtsuho>::customUpdateAddress          = 0x79fb44;
    const DWORD ObjectHook<SokuLib::v2::PlayerUtsuho>::customUpdateRetAddress       = 0x7af33e;
    const DWORD ObjectHook<SokuLib::v2::PlayerSuwako>::customUpdateAddress          = 0x7667BC;
    const DWORD ObjectHook<SokuLib::v2::PlayerSuwako>::customUpdateRetAddress       = 0x7678c0;
    const DWORD ObjectHook<SokuLib::v2::PlayerNamazu>::customUpdateAddress          = 0x7d4a6c;
    const DWORD ObjectHook<SokuLib::v2::PlayerNamazu>::customUpdateRetAddress       = 0x7ea106;

    struct RollCallbacks {
        unsigned int (*saveState)();
        void (*loadStatePre)(size_t frame, unsigned int);
        void (*loadStatePost)(unsigned int);
        void (*freeState)(unsigned int);
    };
    using SetCallbacks_t = void (*)(const RollCallbacks*);
    bool rollInit = false;
}

static void lua_serialize(std::ostream& stream, lua_State* L) {
    switch(lua_type(L, -1)) {
        default:
            Logger::Error("Lua Serialize: Unsupported lua type.");
        case LUA_TNIL: stream.put(LUA_TNIL); break;
        case LUA_TNUMBER: {
            stream.put(LUA_TNUMBER);
            auto v = lua_tonumber(L, -1);
            stream.write((char*)&v, sizeof(v));
        } break;
        case LUA_TBOOLEAN: {
            stream.put(LUA_TBOOLEAN);
            bool v = lua_toboolean(L, -1);
            stream.write((char*)&v, sizeof(v));
        } break;
        case LUA_TSTRING: {
            stream.put(LUA_TSTRING);
            size_t size;
            auto v = lua_tolstring(L, -1, &size);
            stream.write((char*)&size, sizeof(size));
            stream.write(v, size);
        } break;
        case LUA_TLIGHTUSERDATA: {
            stream.put(LUA_TLIGHTUSERDATA);
            auto v = lua_topointer(L, -1);
            stream.write((char*)&v, sizeof(v));
        } break;
        case LUA_TTABLE: {
            stream.put(LUA_TTABLE);
            lua_pushnil(L);
            while(lua_next(L, -2)) {
                lua_pushvalue(L, -2);
                lua_serialize(stream, L);
                lua_serialize(stream, L);
            }
            stream.put(-LUA_TTABLE);
        } break;
    } lua_pop(L, 1);
}

static void lua_deserialize(std::istream& stream, lua_State* L) {
    switch(stream.get()) {
        default: 
            Logger::Error("Lua Serialize: Unsupported lua type.");
        case LUA_TNIL: lua_pushnil(L); break;
        case LUA_TNUMBER: {
            lua_Number v;
            stream.read((char*)&v, sizeof(v));
            lua_pushnumber(L, v);
        } break;
        case LUA_TBOOLEAN: {
            bool v;
            stream.read((char*)&v, sizeof(v));
            lua_pushboolean(L, v);
        } break;
        case LUA_TSTRING: {
            size_t size;
            stream.read((char*)&size, sizeof(size));
            char* buffer = new char[size];
            stream.read(buffer, size);
            lua_pushlstring(L, buffer, size);
            delete[] buffer;
        } break;
        case LUA_TLIGHTUSERDATA: {
            void* v;
            stream.read((char*)&v, sizeof(v));
            lua_pushlightuserdata(L, v);
        } break;
        case LUA_TTABLE: {
            lua_newtable(L);
            while(stream.peek() != (unsigned char)-LUA_TTABLE) {
                lua_deserialize(stream, L);
                lua_deserialize(stream, L);
                lua_settable(L, -3);
            } stream.get();
        } break;
    }
}

static unsigned int roll_saveState() {
    auto stream = new std::stringstream();
    size_t players = playerData.size();
    stream->write((char*)&players, sizeof(players));

    for (auto& data : playerData) {
        lua_State* L = data.first.first;
        SokuLib::v2::Player* player = data.first.second;
        auto& userdata = data.second;

        stream->write((char*)&L, sizeof(L));
        stream->write((char*)&player, sizeof(player));
        userdata.push(L);
        lua_serialize(*stream, L);
    }

    return (unsigned int)stream;
}

static void roll_loadStatePre(size_t frame, unsigned int address) {
    const auto stream = reinterpret_cast<std::stringstream*>(address);
    playerData.clear();
    stream->clear();
    stream->seekg(0);
    size_t players; stream->read((char*)&players, sizeof(players));

    for (int i = 0; i < players; ++i) {
        lua_State* L; stream->read((char*)&L, sizeof(L));
        SokuLib::v2::Player* player; stream->read((char*)&player, sizeof(player));
        lua_deserialize(*stream, L);
        playerData.insert_or_assign(std::pair(L, player), LuaRef::fromStack(L));
    }
}

static void roll_loadStatePost(unsigned int address) { return; }

static void roll_freeState(unsigned int address) {
    delete (std::stringstream*) address;
}

static void __fastcall onBattleInit_replFn(void* self, int unused, int other);
using onBattleInitHook = ShadyLua::CallHook<0x00479556, onBattleInit_replFn>;
onBattleInitHook::typeFn onBattleInitHook::origFn = reinterpret_cast<onBattleInitHook::typeFn>(0x0046b450);
static void __fastcall onBattleInit_replFn(void* self, int unused, int other) {
    onBattleInitHook::origFn(self, unused, other);
    playerData.clear();
}

void ShadyLua::RemoveBattleEvents(ShadyLua::LuaScript* script) {
    // TODO listeners lock
    for (auto list : allListeners)
        for (auto iter = list->begin(); iter != list->end();) {
            if (iter->script == script) iter = list->erase(iter);
            else ++iter;
        }

    auto iter = playerData.begin(); while (iter != playerData.end()) {
        if (script->L == iter->first.first) iter = playerData.erase(iter);
        else ++iter;
    }
}

template<class T>
static inline void addObjectListener(const HookData& data) {
    ShadyLua::initHook<onBattleInitHook>();
    const DWORD ADDR = SokuLib::_vtable_info<T>::baseAddr;
    if (!ShadyLua::hooks[ADDR]) {
        ShadyLua::hooks[ADDR].reset(new ObjectHook<T>());
        allListeners.push_back(&ObjectHook<T>::listeners);
    }
    ObjectHook<T>::listeners.push_back(data);

    if (!rollInit) {
        rollInit = true;
        auto module = LoadLibraryA("giuroll");
        if (!module) return;
        auto SetCallbacks = reinterpret_cast<SetCallbacks_t>(GetProcAddress(module, "addRollbackCb"));
        if (!SetCallbacks) return;
        RollCallbacks callbacks = {
            roll_saveState,
            roll_loadStatePre,
            roll_loadStatePost,
            roll_freeState,
        }; SetCallbacks(&callbacks);
    }
}

static int battle_replaceCharacter(lua_State* L) {
    unsigned int c = luaL_checkinteger(L, 1);
    HookData data(L);
    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        data.updateHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        data.initSeqHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (lua_isfunction(L, 4)) {
        lua_pushvalue(L, 4);
        data.initHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    switch(c) {
        case SokuLib::CHARACTER_REIMU:      addObjectListener<SokuLib::v2::PlayerReimu>(data); break;
        case SokuLib::CHARACTER_MARISA:     addObjectListener<SokuLib::v2::PlayerMarisa>(data); break;
        case SokuLib::CHARACTER_SAKUYA:     addObjectListener<SokuLib::v2::PlayerSakuya>(data); break;
        case SokuLib::CHARACTER_ALICE:      addObjectListener<SokuLib::v2::PlayerAlice>(data); break;
        case SokuLib::CHARACTER_PATCHOULI:  addObjectListener<SokuLib::v2::PlayerPatchouli>(data); break;
        case SokuLib::CHARACTER_YOUMU:      addObjectListener<SokuLib::v2::PlayerYoumu>(data); break;
        case SokuLib::CHARACTER_REMILIA:    addObjectListener<SokuLib::v2::PlayerRemilia>(data); break;
        case SokuLib::CHARACTER_YUYUKO:     addObjectListener<SokuLib::v2::PlayerYuyuko>(data); break;
        case SokuLib::CHARACTER_YUKARI:     addObjectListener<SokuLib::v2::PlayerYukari>(data); break;
        case SokuLib::CHARACTER_SUIKA:      addObjectListener<SokuLib::v2::PlayerSuika>(data); break;
        case SokuLib::CHARACTER_UDONGE:     addObjectListener<SokuLib::v2::PlayerUdonge>(data); break;
        case SokuLib::CHARACTER_AYA:        addObjectListener<SokuLib::v2::PlayerAya>(data); break;
        case SokuLib::CHARACTER_KOMACHI:    addObjectListener<SokuLib::v2::PlayerKomachi>(data); break;
        case SokuLib::CHARACTER_IKU:        addObjectListener<SokuLib::v2::PlayerIku>(data); break;
        case SokuLib::CHARACTER_TENSHI:     addObjectListener<SokuLib::v2::PlayerTenshi>(data); break;
        case SokuLib::CHARACTER_SANAE:      addObjectListener<SokuLib::v2::PlayerSanae>(data); break;
        case SokuLib::CHARACTER_CIRNO:      addObjectListener<SokuLib::v2::PlayerChirno>(data); break;
        case SokuLib::CHARACTER_MEILING:    addObjectListener<SokuLib::v2::PlayerMeirin>(data); break;
        case SokuLib::CHARACTER_UTSUHO:     addObjectListener<SokuLib::v2::PlayerUtsuho>(data); break;
        case SokuLib::CHARACTER_SUWAKO:     addObjectListener<SokuLib::v2::PlayerSuwako>(data); break;
        case SokuLib::CHARACTER_NAMAZU:     addObjectListener<SokuLib::v2::PlayerNamazu>(data); break;
        default: luaL_error(L, "Character not found: %d", c);
    }
    return 0;
}

static int battle_replaceObjects(lua_State* L) {
    unsigned int c = luaL_checkinteger(L, 1);
    HookData data(L);
    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        data.updateHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        data.initSeqHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    switch(c) {
        case SokuLib::CHARACTER_REIMU:      addObjectListener<SokuLib::v2::GameObjectReimu>(data); break;
        case SokuLib::CHARACTER_MARISA:     addObjectListener<SokuLib::v2::GameObjectMarisa>(data); break;
        case SokuLib::CHARACTER_SAKUYA:     addObjectListener<SokuLib::v2::GameObjectSakuya>(data); break;
        case SokuLib::CHARACTER_ALICE:      addObjectListener<SokuLib::v2::GameObjectAlice>(data); break;
        case SokuLib::CHARACTER_PATCHOULI:  addObjectListener<SokuLib::v2::GameObjectPatchouli>(data); break;
        case SokuLib::CHARACTER_YOUMU:      addObjectListener<SokuLib::v2::GameObjectYoumu>(data); break;
        case SokuLib::CHARACTER_REMILIA:    addObjectListener<SokuLib::v2::GameObjectRemilia>(data); break;
        case SokuLib::CHARACTER_YUYUKO:     addObjectListener<SokuLib::v2::GameObjectYuyuko>(data); break;
        case SokuLib::CHARACTER_YUKARI:     addObjectListener<SokuLib::v2::GameObjectYukari>(data); break;
        case SokuLib::CHARACTER_SUIKA:      addObjectListener<SokuLib::v2::GameObjectSuika>(data); break;
        case SokuLib::CHARACTER_UDONGE:     addObjectListener<SokuLib::v2::GameObjectUdonge>(data); break;
        case SokuLib::CHARACTER_AYA:        addObjectListener<SokuLib::v2::GameObjectAya>(data); break;
        case SokuLib::CHARACTER_KOMACHI:    addObjectListener<SokuLib::v2::GameObjectKomachi>(data); break;
        case SokuLib::CHARACTER_IKU:        addObjectListener<SokuLib::v2::GameObjectIku>(data); break;
        case SokuLib::CHARACTER_TENSHI:     addObjectListener<SokuLib::v2::GameObjectTenshi>(data); break;
        case SokuLib::CHARACTER_SANAE:      addObjectListener<SokuLib::v2::GameObjectSanae>(data); break;
        case SokuLib::CHARACTER_CIRNO:      addObjectListener<SokuLib::v2::GameObjectChirno>(data); break;
        case SokuLib::CHARACTER_MEILING:    addObjectListener<SokuLib::v2::GameObjectMeirin>(data); break;
        case SokuLib::CHARACTER_UTSUHO:     addObjectListener<SokuLib::v2::GameObjectUtsuho>(data); break;
        case SokuLib::CHARACTER_SUWAKO:     addObjectListener<SokuLib::v2::GameObjectSuwako>(data); break;
        case SokuLib::CHARACTER_NAMAZU:     addObjectListener<SokuLib::v2::GameObjectNamazu>(data); break;
        default: luaL_error(L, "Character not found: %d", c);
    }
    return 0;
}

static int battle_GameObjectBase_getPtr(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    lua_pushinteger(L, (int)o);
    return 1;
}

static int battle_GameObjectBase_getOwner(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    push(L, (SokuLib::v2::Player*)o->gameData.owner);
    return 1;
}

static int battle_GameObjectBase_getAlly(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    push(L, (SokuLib::v2::Player*)o->gameData.ally);
    return 1;
}

static int battle_GameObjectBase_getOpponent(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    push(L, (SokuLib::v2::Player*)o->gameData.opponent);
    return 1;
}

static int battle_GameObjectBase_getDir(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    lua_pushinteger(L, o->direction);
    return 1;
}

static int battle_GameObjectBase_setDir(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    int value = luaL_checkinteger(L, 2);
    o->direction = (SokuLib::Direction)value;
    return 0;
}

static int battle_GameObjectBase_getShadowOn(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    lua_pushboolean(L, o->isActive);
    return 1;
}
static int battle_GameObjectBase_setShadowOn(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObjectBase*>::get(L, 1);
    o->isActive = lua_toboolean(L, 2);
    if (o->isActive)
        o->unknown138 = -6;
    return 0;
}

static ShadyLua::Renderer::Effect* battle_GameObjectBase_createEffect(SokuLib::v2::GameObjectBase* object, lua_State* L) {
    auto fxmanager = *reinterpret_cast<SokuLib::v2::EffectManager_Effect**>(0x8985f0);
    int actionId = luaL_checkinteger(L, 2);
    int index = 3;
    float x, y;
    if (Stack<SokuLib::Vector2f>::isInstance(L, index)) {
        auto xy = Stack<SokuLib::Vector2f>::get(L, index++);//3
        x = xy.x; y = xy.y;
    }
    //else if (lua_istable(L, index)) {
    //    auto xy = Stack<std::vector<float>>::get(L, index++);//3
    //    x = xy[0]; y = xy[1];
    //}
    else {
        x = luaL_optnumber(L, index++, object->position.x);//3
        y = luaL_optnumber(L, index++, object->position.y);//4
    }
    char direction = luaL_optinteger(L, index++, object->direction);
    char layer = luaL_optinteger(L, index++, 1);
    auto fx = fxmanager->CreateEffect(actionId, x, y, direction, layer, (int)object);
    return (ShadyLua::Renderer::Effect*)fx;
}

static void battle_GameObjectBase_setHitBoxData(SokuLib::v2::GameObjectBase* object, lua_State* L) {
    if (lua_gettop(L) <= 1) {
        if (object->customHitBox) SokuLib::Delete(object->customHitBox);
        object->customHitBox = 0;
        return;
    }

    int left      = luaL_optinteger(L, 2, 0);
    int top       = luaL_optinteger(L, 3, 0);
    int right     = luaL_optinteger(L, 4, 0);
    int bottom    = luaL_optinteger(L, 5, 0);
    short angle   = luaL_optinteger(L, 6, 0);
    short anchorX = luaL_optinteger(L, 7, 0);
    short anchorY = luaL_optinteger(L, 8, 0);

    object->setHitBoxData(left, top, right, bottom, angle, anchorX, anchorY);
}

static int __fastcall battle_GameObjectBase_getHitBoxData(SokuLib::v2::GameObjectBase* object, int unused, lua_State* L) {
    if (!object->customHitBox) return 0;
    luabridge::push(L, object->customHitBox->box.left);
    luabridge::push(L, object->customHitBox->box.top);
    luabridge::push(L, object->customHitBox->box.right);
    luabridge::push(L, object->customHitBox->box.bottom);
    luabridge::push(L, object->customHitBox->rotation);
    luabridge::push(L, object->customHitBox->rotationAnchor.x);
    luabridge::push(L, object->customHitBox->rotationAnchor.y);
    return 7;
}

static std::string battle_GameObject_getCustomData(SokuLib::v2::GameObject* object, lua_State* L) {
    int size = luaL_checkinteger(L, 2);
    return std::string((const char*)object->customData, size);
}
static ShadyLua::CustomDataProxy battle_CustomData_fromPtr(int addr) {
    return ShadyLua::CustomDataProxy((void*)addr);
}
static int battle_GameObject_getCustomDataProxy(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObject*>::get(L, 1);
    Stack<ShadyLua::CustomDataProxy>::push(L, ShadyLua::CustomDataProxy(o->customData));
    return 1;
}
static int battle_GameObject_setCustomDataProxy(lua_State* L) {
    auto o = Stack<SokuLib::v2::GameObject*>::get(L, 1);
    if (Stack<ShadyLua::CustomDataProxy>::isInstance(L, 2)) {
        // TODO ignore on same addr
        //      and maybe replace otherwise
        //      take care with memory leaks
    }
    else if (lua_istable(L, 2)) {
        LuaRef ref = LuaRef::fromStack(L, 2);
        auto len = ref.length();
        for (int i = 0; i < len; ++i) o->customData[i] = ref[i + 1].cast<float>();
    }
    else return luaL_argerror(L, 2, "expected a table element.");
    return 0;
}

static LuaRef battle_GameObject_getChildren(SokuLib::v2::GameObject* object, lua_State* L) {
    LuaRef table = newTable(L);
    int i = 0; for (auto child : object->childrenB) {
        table[++i] = child;
    }
    return table;
}

template <class Class, SokuLib::v2::GameObject* (Class::*Func)(short, float, float, char, char, float*, unsigned int)>
static SokuLib::v2::GameObject* battle_createObject(Class* base, lua_State* L) {
    int index = 2;
    int actionId = luaL_checkinteger(L, index++);

    float x, y;
    if (Stack<SokuLib::Vector2f>::isInstance(L, index)) {
        auto xy = Stack<SokuLib::Vector2f>::get(L, index++);
        x = xy.x; y = xy.y;
    } else {
        x = luaL_optnumber(L, index++, base->position.x);
        y = luaL_optnumber(L, index++, base->position.y);
    }

    char direction = luaL_optinteger(L, index++, base->direction);
    char layer = luaL_optinteger(L, index++, 1);

    size_t dataSize = 0;
    const char* data = nullptr;
    std::vector<float> fdata;
    int sequenceId = 0;
    switch (lua_type(L, index)) {
    case LUA_TNUMBER:
        sequenceId = luaL_checkinteger(L, index++);
    case LUA_TNONE:
        fdata.assign(3, 0); fdata[2] = sequenceId;
        data = reinterpret_cast<const char*>(fdata.data());
        dataSize = 3;
        break;
    case LUA_TTABLE:
        fdata = Stack<std::vector<float>>::get(L, index++);
        data = reinterpret_cast<const char*>(fdata.data());
        dataSize = fdata.size();
        break;
    case LUA_TSTRING:
        data = lua_tolstring(L, index++, &dataSize);
        dataSize /= 4;
        break;
    default: luaL_argerror(L, index++, "optional argument can only accept integer, string or table of numbers");
    }

    return (base->*Func)(actionId, x, y, direction, layer, dataSize ? (float*)data : nullptr, dataSize);
}

static int battle_Player_handGetId(SokuLib::v2::Player* player, lua_State* L) {
    unsigned int index = luaL_checkinteger(L, 2);
    if (index >= player->handInfo.hand.size()) return luaL_error(L, "index outside range.");
    auto& card = player->handInfo.hand.at(index);
    return card.id;
}

static int battle_Player_handGetCost(SokuLib::v2::Player* player, lua_State* L) {
    unsigned int index = luaL_checkinteger(L, 2);
    if (index >= player->handInfo.hand.size()) return luaL_error(L, "index outside range.");
    auto& card = player->handInfo.hand.at(index);
    return card.cost;
}

template <int id>
static int battle_Manager_getPlayer(lua_State* L) {
    auto manager = Stack<SokuLib::BattleManager*>::get(L, 1);
    push(L, *(SokuLib::v2::Player**)((int)manager+0x0C+id*4));
    return 1;
}

static int battle_getManager(lua_State* L) {
    push (L, &SokuLib::getBattleMgr());
    return 1;
}

static int battle_random(lua_State* L) {
    auto i = luaL_optinteger(L, 1, -1);
    unsigned int ret = i < 0 ? SokuLib::rand() : SokuLib::rand(i);
    lua_pushinteger(L, ret);
    return 1;
}

template <typename T> static inline T& castFromPtr(size_t addr) { return *(T*)addr; }

void ShadyLua::LualibBattle(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("battle")
            .beginClass<SokuLib::RenderInfo>("RenderInfo")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::RenderInfo>)
                .addConstructor<void (*)()>()
                .addData<unsigned int>("color", &SokuLib::RenderInfo::color, true)
                .addData<int>("shaderType", &SokuLib::RenderInfo::shaderType, true)
                .addData<unsigned int>("shaderColor", &SokuLib::RenderInfo::shaderColor, true)
                .addProperty("scale", &SokuLib::RenderInfo::scale, true)
                .addData("xRotation", &SokuLib::RenderInfo::xRotation, true)
                .addData("yRotation", &SokuLib::RenderInfo::yRotation, true)
                .addData("zRotation", &SokuLib::RenderInfo::zRotation, true)
            .endClass()
            .beginClass<ShadyLua::CustomDataProxy>("CustomData")
                .addStaticFunction("fromPtr", battle_CustomData_fromPtr)
                .addFunction("__index", &ShadyLua::CustomDataProxy::__index)
                .addFunction("__newindex", &ShadyLua::CustomDataProxy::__newindex)
                .addFunction("__len", &ShadyLua::CustomDataProxy::__len)
            .endClass()
            .beginClass<SokuLib::v2::GameObjectBase>("ObjectBase")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::v2::GameObjectBase>)
                .addProperty("ptr", battle_GameObjectBase_getPtr, 0)
                .addProperty("center", &SokuLib::v2::GameObjectBase::center, true)
                .addProperty("position", &SokuLib::v2::GameObjectBase::position, true)
                .addProperty("speed", &SokuLib::v2::GameObjectBase::speed, true)
                .addProperty("gravity", &SokuLib::v2::GameObjectBase::gravity, true)
                .addProperty("direction", battle_GameObjectBase_getDir, battle_GameObjectBase_setDir)
                .addProperty("renderInfo", &SokuLib::v2::GameObjectBase::renderInfos, true)
                .addProperty("isGui", &SokuLib::v2::GameObjectBase::isGui, true)
                .addProperty("shadowOn", battle_GameObjectBase_getShadowOn, battle_GameObjectBase_setShadowOn)
                .addProperty("shadowOffset", &SokuLib::v2::GameObjectBase::unknown138, true)
                .addProperty("actionId", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.actionId), false)
                .addProperty("sequenceId", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.sequenceId), false)
                .addProperty("poseId", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.poseId), false)
                .addProperty("poseFrame", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.poseFrame), false)
                .addProperty("currentFrame", MEMBER_ADDRESS(unsigned int, SokuLib::v2::GameObjectBase, frameState.currentFrame), false)
                .addProperty("sequenceSize", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.sequenceSize), false)
                .addProperty("poseDuration", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.poseDuration), false)
                .addProperty("owner", battle_GameObjectBase_getOwner, 0)
                .addProperty("ally", battle_GameObjectBase_getAlly, 0)
                .addProperty("opponent", battle_GameObjectBase_getOpponent, 0)

                .addProperty("hp", &SokuLib::v2::GameObjectBase::hp, true)
                .addProperty("maxHp", &SokuLib::v2::GameObjectBase::maxHP, true)
                .addProperty("skillIndex", &SokuLib::v2::GameObjectBase::skillIndex, true)
                .addProperty("collisionType", MEMBER_ADDRESS(int, SokuLib::v2::GameObjectBase, collisionType), true)
                .addProperty("collisionLimit", MEMBER_ADDRESS(unsigned char, SokuLib::v2::GameObjectBase, collisionLimit), true)
                .addProperty("hitStop", &SokuLib::v2::GameObjectBase::hitStop, true)

                .addProperty<float, float>("groundHeight", &SokuLib::v2::GameObjectBase::getGroundHeight, 0)

                .addFunction("createEffect", battle_GameObjectBase_createEffect)
                .addFunction("setActionSequence", &SokuLib::v2::GameObjectBase::setActionSequence)
                .addFunction("setAction", &SokuLib::v2::GameObjectBase::setAction)
                .addFunction("setSequence", &SokuLib::v2::GameObjectBase::setSequence)
                .addFunction("setPose", &SokuLib::v2::GameObjectBase::setPose)
                .addFunction("advanceFrame", &SokuLib::v2::GameObjectBase::advanceFrame)
                .addFunction("resetForces", &SokuLib::v2::GameObjectBase::resetForces)
                .addFunction("isOnGround", &SokuLib::v2::GameObjectBase::isOnGround)
                .addFunction("setHitBoxData", battle_GameObjectBase_setHitBoxData)
                .addFunction("getHitBoxData", SokuLib::union_cast<int (SokuLib::v2::GameObjectBase::*)(lua_State*)>(battle_GameObjectBase_getHitBoxData))
            .endClass()

            .deriveClass<SokuLib::v2::GameObject, SokuLib::v2::GameObjectBase>("Object")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::v2::GameObject>)
                .addProperty("lifetime", &SokuLib::v2::GameObject::lifetime, true)
                .addProperty("layer", &SokuLib::v2::GameObject::layer, true)
                .addProperty("parentPlayerB", &SokuLib::v2::GameObject::parentPlayerB)
                .addProperty("parentObjectB", &SokuLib::v2::GameObject::parentB)
                
                .addProperty("customData", battle_GameObject_getCustomDataProxy, battle_GameObject_setCustomDataProxy)
                .addProperty("gpShort", ShadyLua::ArrayRef_from(&SokuLib::v2::GameObject::gpShort), true)
                .addProperty("gpFloat", ShadyLua::ArrayRef_from(&SokuLib::v2::GameObject::gpFloat), true)

                .addFunction("getChildrenB", battle_GameObject_getChildren)
                .addFunction("setTail", &SokuLib::v2::GameObject::setTail)
                .addFunction("getCustomData", battle_GameObject_getCustomData)
                .addFunction("checkGrazed", &SokuLib::v2::GameObject::checkGrazed)
                .addFunction("checkProjectileHit", &SokuLib::v2::GameObject::checkProjectileHit)
                .addFunction("checkTurnIntoCrystals", &SokuLib::v2::GameObject::checkTurnIntoCrystals)

                .addFunction("createObject", battle_createObject<SokuLib::v2::GameObject, &SokuLib::v2::GameObject::createObject>)
                .addFunction("createChild", battle_createObject<SokuLib::v2::GameObject, &SokuLib::v2::GameObject::createChild>)
            .endClass()

            .deriveClass<SokuLib::v2::Player, SokuLib::v2::GameObjectBase>("Player")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::v2::Player>)
                .addProperty("character", MEMBER_ADDRESS(int, SokuLib::v2::Player, characterIndex), false)
                .addProperty("isRight", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, teamId), false)
                .addProperty("teamId", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, teamId), false)
                .addProperty("paletteId", &SokuLib::v2::Player::paletteId, false)
                .addProperty("unknown4A6", &SokuLib::v2::Player::spellStopCounter, true)
                .addProperty("spellStopCounter", &SokuLib::v2::Player::spellStopCounter, true)
                .addProperty("groundDashCount", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, groundDashCount), true)
                .addProperty("airDashCount", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, airDashCount), true)
                .addProperty("currentSpirit", &SokuLib::v2::Player::currentSpirit, false)
                .addProperty("timeStop", &SokuLib::v2::Player::timeStop, false)

                .addProperty("comboRate", &SokuLib::v2::Player::comboRate, false)
                .addProperty("comboCount", &SokuLib::v2::Player::comboCount, false)
                .addProperty("comboDamage", &SokuLib::v2::Player::comboDamage, false)
                .addProperty("comboLimit", &SokuLib::v2::Player::comboLimit, false)
                .addProperty("untech", &SokuLib::v2::Player::untech, false)
                .addProperty("skillCancelCount", &SokuLib::v2::Player::skillCancelCount, true)

                .addProperty("meleeInvulTimer", &SokuLib::v2::Player::meleeInvulTimer, true)
                .addProperty("grabInvulTimer", &SokuLib::v2::Player::grabInvulTimer, true)
                .addProperty("projectileInvulTimer", &SokuLib::v2::Player::projectileInvulTimer, true)
                .addProperty("grazeTimer", &SokuLib::v2::Player::grazeTimer, true)
                .addProperty("confusionDebuffTimer", &SokuLib::v2::Player::confusionDebuffTimer, true)
                .addProperty("SORDebuffTimer", &SokuLib::v2::Player::SORDebuffTimer, true)
                .addProperty("healCharmTimer", &SokuLib::v2::Player::healCharmTimer, true)
                
                .addProperty("input", MEMBER_ADDRESS(SokuLib::KeyInputLight, SokuLib::v2::Player, inputData.keyInput), false)
                .addProperty("inputBuffered", MEMBER_ADDRESS(SokuLib::KeyInputLight, SokuLib::v2::Player, inputData.bufferedKeyInput), false)
                .addProperty("gpShort", ShadyLua::ArrayRef_from(&SokuLib::v2::Player::gpShort), true)
                .addProperty("gpFloat", ShadyLua::ArrayRef_from(&SokuLib::v2::Player::gpFloat), true)

                .addProperty("handCount", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, handInfo.cardCount), false)
                .addFunction("handGetId", battle_Player_handGetId)
                .addFunction("handGetCost", battle_Player_handGetCost)

                .addFunction("createObject", battle_createObject<SokuLib::v2::Player, &SokuLib::v2::Player::createObject>)
                .addFunction("updateGroundMovement", &SokuLib::v2::Player::updateGroundMovement)
                .addFunction("updateAirMovement", &SokuLib::v2::Player::decideShotAngle)
                .addFunction("decideShotAngle", &SokuLib::v2::Player::decideShotAngle)
                .addFunction("handleCardSwitch", &SokuLib::v2::Player::handleCardSwitch)
                .addFunction("useSystemCard", &SokuLib::v2::Player::useSystemCard)
                .addFunction("canSpendSpirit", &SokuLib::v2::Player::canSpendSpirit)
                .addFunction("getMoveLock", &SokuLib::v2::Player::getMoveLock)
                .addFunction("canActivateCard", &SokuLib::v2::Player::canActivateCard)
                .addFunction("isGrounded", &SokuLib::v2::Player::isGrounded)

                .addFunction("handleHJ", &SokuLib::v2::Player::handleHJ)
                .addFunction("handleHJInput", &SokuLib::v2::Player::handleHJInput)
                .addFunction("handleGroundDash", &SokuLib::v2::Player::handleGroundDash)
                .addFunction("handleGroundBE", &SokuLib::v2::Player::handleGroundBE)
                .addFunction("handleAirBE", &SokuLib::v2::Player::handleAirBE)
                .addFunction("handleFwdAirDash", &SokuLib::v2::Player::handleFwdAirDash)
                .addFunction("handleBackAirDash", &SokuLib::v2::Player::handleBackAirDash)
                .addFunction("handleNormalFlight", &SokuLib::v2::Player::handleNormalFlight)

                .addFunction("useSpellCard", &SokuLib::v2::Player::useSpellCard)
                .addFunction("useSkill", &SokuLib::v2::Player::useSkill)
                .addFunction("addCardMeter", &SokuLib::v2::Player::addCardMeter)
                .addFunction("applyGroundMechanics", &SokuLib::v2::Player::applyGroundMechanics)
                .addFunction("applyAirMechanics", &SokuLib::v2::Player::applyAirMechanics)
                .addFunction("playSFX", &SokuLib::v2::Player::playSFX)
                .addFunction("checkTurnAround", &SokuLib::v2::Player::checkTurnAround)
                .addFunction("playSpellBackground", &SokuLib::v2::Player::playSpellBackground)
                .addFunction("consumeSpirit", &SokuLib::v2::Player::consumeSpirit)
                .addFunction("consumeCard", &SokuLib::v2::Player::consumeCard)
                .addFunction("eventSkillUse", &SokuLib::v2::Player::eventSkillUse)
                .addFunction("eventSpellUse", &SokuLib::v2::Player::eventSpellUse)
                .addFunction("eventWeatherCycle", &SokuLib::v2::Player::eventWeatherCycle)
                .addFunction("unknown46d950", &SokuLib::v2::Player::FUN_0046d950)
            .endClass()

            .beginClass<SokuLib::PlayerInfo>("PlayerInfo")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::PlayerInfo>)
                .addProperty("character", MEMBER_ADDRESS(int, SokuLib::PlayerInfo, character), true)
                .addProperty("isRight", &SokuLib::PlayerInfo::isRight, false)
                .addProperty("teamId", &SokuLib::PlayerInfo::isRight, false)
                .addProperty("palette", MEMBER_ADDRESS(unsigned char, SokuLib::PlayerInfo, palette), true)
                .addProperty("paletteId", MEMBER_ADDRESS(unsigned char, SokuLib::PlayerInfo, palette), true)
                .addProperty("inputType", &SokuLib::PlayerInfo::inputType, false)
                .addProperty("deckId", &SokuLib::PlayerInfo::deck, true)
            .endClass()

            .beginClass<SokuLib::GameStartParams>("GameParams")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::GameStartParams>)
                .addProperty("difficulty", MEMBER_ADDRESS(int, SokuLib::GameStartParams, offset_0x00), true)
                .addProperty("stageId", &SokuLib::GameStartParams::stageId, true)
                .addProperty("musicId", &SokuLib::GameStartParams::musicId, true)
                .addProperty("player1", &SokuLib::GameStartParams::leftPlayerInfo, true)
                .addProperty("player2", &SokuLib::GameStartParams::rightPlayerInfo, true)
                .addProperty("randomSeed", &SokuLib::GameStartParams::randomSeed, false)
            .endClass()

            .beginClass<SokuLib::BattleManager>("Manager")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::BattleManager>)
                .addProperty("player1", battle_Manager_getPlayer<0>, 0)
                .addProperty("player2", battle_Manager_getPlayer<1>, 0)
                .addProperty("frameCount", &SokuLib::BattleManager::frameCount, false)
                .addProperty("matchState", MEMBER_ADDRESS(unsigned char, SokuLib::BattleManager, matchState), false)
            .endClass()

            .addProperty("manager", battle_getManager, 0)
            .addProperty("gameParams", &SokuLib::gameParams)
            .addProperty("activeWeather", (int*)&SokuLib::activeWeather)
            .addProperty("displayedWeather", (int*)&SokuLib::displayedWeather)

            .addFunction("replaceCharacter", battle_replaceCharacter)
            .addFunction("replaceObjects", battle_replaceObjects)
            .addFunction("random", battle_random)
        .endNamespace()
    ;
}