#include "../lualibs.hpp"
#include "../script.hpp"
#include "gui.hpp"
#include <LuaBridge/LuaBridge.h>

using namespace luabridge;

#define MEMBER_ADDRESS(u,s,m) SokuLib::union_cast<u s::*>(&reinterpret_cast<char const volatile&>(((s*)0)->m))

namespace {
    struct HookData {
        int updateHandler = LUA_REFNIL;
        int initSeqHandler = LUA_REFNIL;
        // int update2Handler = LUA_REFNIL;
        int initHandler = LUA_REFNIL;
        ShadyLua::LuaScript* script;
        luabridge::LuaRef data;

        inline HookData(lua_State* L) : data(newTable(L)), script(ShadyLua::ScriptMap[L]) {}
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
                } else luabridge::push(L, (SokuLib::v2::GameObject*)object);
                lua_pushinteger(L, object->frameState.actionId);
                luabridge::push(L, data.data);

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
                } else luabridge::push(L, (SokuLib::v2::GameObject*)object);
                lua_pushinteger(L, object->frameState.actionId);
                luabridge::push(L, data.data);

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
                } else luabridge::push(L, (SokuLib::v2::GameObject*)object);
                luabridge::push(L, data.data);

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
}

void ShadyLua::RemoveBattleEvents(ShadyLua::LuaScript* script) {
    // TODO listeners lock
    for (auto list : allListeners)
        for (auto iter = list->begin(); iter != list->end();) {
            if (iter->script == script) iter = list->erase(iter);
            else ++iter;
        }
}

template<class T>
static inline void addObjectListener(const HookData& data) {
    const DWORD ADDR = SokuLib::_vtable_info<T>::baseAddr;
    if (!ShadyLua::hooks[ADDR]) {
        ShadyLua::hooks[ADDR].reset(new ObjectHook<T>());
        allListeners.push_back(&ObjectHook<T>::listeners);
    }
    ObjectHook<T>::listeners.push_back(data);
}

static int battle_replaceCharacter(lua_State* L) {
    Logger::Warning("ALERT: a mod that can cause desync was loaded!");
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
    Logger::Warning("ALERT: a mod that can cause desync was loaded!");
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

static ShadyLua::Renderer::Effect* battle_GameObjectBase_createEffect(SokuLib::v2::GameObjectBase* object, lua_State* L) {
    auto fxmanager = *reinterpret_cast<SokuLib::v2::EffectManager_Effect**>(0x8985f0);
    int actionId = luaL_checkinteger(L, 2);
    float x = luaL_checknumber(L, 3);
    float y = luaL_checknumber(L, 4);
    char direction = luaL_optinteger(L, 5, object->direction);
    char layer = luaL_optinteger(L, 6, 1);
    auto fx = fxmanager->CreateEffect(actionId, x, y, (SokuLib::Direction)direction, layer, (int)object);
    return (ShadyLua::Renderer::Effect*)fx;
}

static std::string battle_GameObject_getCustomData(SokuLib::v2::GameObject* object, lua_State* L) {
    int size = luaL_checkinteger(L, 2);
    return std::string((const char*)object->customData, size);
}

static SokuLib::v2::GameObject* battle_Player_createObject(SokuLib::v2::Player* player, lua_State* L) {
    int actionId = luaL_checkinteger(L, 2);
    float x = luaL_checknumber(L, 3);
    float y = luaL_checknumber(L, 4);
    char direction = luaL_optinteger(L, 5, player->direction);
    char layer = luaL_optinteger(L, 6, 1);
    size_t dataSize = 0;
    const char* data = luaL_optlstring(L, 7, "", &dataSize);
    return player->objectList->createObject(0, player, (SokuLib::Action)actionId, x, y, (SokuLib::Direction)direction, layer, dataSize ? (void*)data : 0, dataSize);
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

void ShadyLua::LualibBattle(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("battle")
            .beginClass<SokuLib::v2::GameObjectBase>("ObjectBase")
                .addProperty("ptr", battle_GameObjectBase_getPtr, 0)
                .addProperty("position", &SokuLib::v2::GameObjectBase::position, true)
                .addProperty("speed", &SokuLib::v2::GameObjectBase::speed, true)
                .addProperty("gravity", &SokuLib::v2::GameObjectBase::gravity, true)
                .addProperty("direction", battle_GameObjectBase_getDir, battle_GameObjectBase_setDir)
                .addProperty("actionId", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.actionId), false)
                .addProperty("sequenceId", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.sequenceId), false)
                .addProperty("poseId", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.poseId), false)
                .addProperty("poseFrame", MEMBER_ADDRESS(unsigned short, SokuLib::v2::GameObjectBase, frameState.poseFrame), false)
                .addProperty("currentFrame", MEMBER_ADDRESS(unsigned int, SokuLib::v2::GameObjectBase, frameState.currentFrame), false)
                .addProperty("opponent", battle_GameObjectBase_getOpponent, 0)
                .addProperty("hp", &SokuLib::v2::GameObjectBase::HP, true)
                .addProperty("maxHp", &SokuLib::v2::GameObjectBase::MaxHP, true)

                .addProperty<float, float>("groundHeight", &SokuLib::v2::GameObjectBase::getGroundHeight, 0)

                .addFunction("createEffect", battle_GameObjectBase_createEffect)
                .addFunction("setActionSequence", &SokuLib::v2::GameObjectBase::setActionSequence)
                .addFunction("setAction", &SokuLib::v2::GameObjectBase::setAction)
                .addFunction("setSequence", &SokuLib::v2::GameObjectBase::setSequence)
                .addFunction("setPose", &SokuLib::v2::GameObjectBase::setPose)
                .addFunction("advanceFrame", &SokuLib::v2::GameObjectBase::advanceFrame)
                .addFunction("resetForces", &SokuLib::v2::GameObjectBase::resetForces)
                .addFunction("isOnGround", &SokuLib::v2::GameObjectBase::isOnGround)
            .endClass()

            .deriveClass<SokuLib::v2::GameObject, SokuLib::v2::GameObjectBase>("Object")
                .addProperty("lifetime", &SokuLib::v2::GameObject::lifetime, true)

                .addFunction("setTail", &SokuLib::v2::GameObject::setTail)
                .addFunction("getCustomData", battle_GameObject_getCustomData)
            .endClass()

            .deriveClass<SokuLib::v2::Player, SokuLib::v2::GameObjectBase>("Player")
                .addProperty("character", &SokuLib::v2::Player::characterIndex, false)
                .addProperty("collisionType", &SokuLib::v2::Player::collisionType, false)
                .addProperty("collisionLimit", &SokuLib::v2::Player::collisionLimit, true)
                .addProperty("unknown4A6", &SokuLib::v2::Player::spellStopCounter, true)
                .addProperty("spellStopCounter", &SokuLib::v2::Player::spellStopCounter, true)
                .addProperty("groundDashCount", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, groundDashCount), true)
                .addProperty("airDashCount", MEMBER_ADDRESS(unsigned char, SokuLib::v2::Player, airDashCount), true)

                .addFunction("createObject", battle_Player_createObject)
                .addFunction("updateGroundMovement", &SokuLib::v2::Player::updateGroundMovement)
                .addFunction("updateAirMovement", &SokuLib::v2::Player::updateAirMovement)
                .addFunction("addCardMeter", &SokuLib::v2::Player::addCardMeter)
                .addFunction("applyGroundMechanics", &SokuLib::v2::Player::applyGroundMechanics)
                .addFunction("applyAirMechanics", &SokuLib::v2::Player::applyAirMechanics)
                .addFunction("playSFX", &SokuLib::v2::Player::playSFX)
                .addFunction("unknown487C20", &SokuLib::v2::Player::Unknown487C20)
                .addFunction("playSpellBackground", &SokuLib::v2::Player::playSpellBackground)
                .addFunction("consumeSpirit", &SokuLib::v2::Player::consumeSpirit)
                .addFunction("consumeCard", &SokuLib::v2::Player::consumeCard)
                .addFunction("eventSkillUse", &SokuLib::v2::Player::eventSkillUse)
                .addFunction("eventSpellUse", &SokuLib::v2::Player::eventSpellUse)
                .addFunction("eventWeatherCycle", &SokuLib::v2::Player::eventWeatherCycle)
            .endClass()

            .beginClass<SokuLib::PlayerInfo>("PlayerInfo")
                .addProperty("character", (int SokuLib::PlayerInfo::*)&SokuLib::PlayerInfo::character, false)
                .addProperty("isRight", &SokuLib::PlayerInfo::isRight, false)
                .addProperty("teamId", &SokuLib::PlayerInfo::isRight, false)
                .addProperty("palette", &SokuLib::PlayerInfo::palette, false)
                .addProperty("paletteId", &SokuLib::PlayerInfo::palette, false)
                .addProperty("inputType", &SokuLib::PlayerInfo::padding2, false)
                .addProperty("deckId", &SokuLib::PlayerInfo::deck, false)
            .endClass()

            .beginClass<SokuLib::GameStartParams>("GameParams")
                .addProperty("stageId", &SokuLib::GameStartParams::stageId)
                .addProperty("musicId", &SokuLib::GameStartParams::musicId)
                .addProperty("player1", &SokuLib::GameStartParams::leftPlayerInfo, false)
                .addProperty("player2", &SokuLib::GameStartParams::rightPlayerInfo, false)
            .endClass()

            .beginClass<SokuLib::BattleManager>("Manager")
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