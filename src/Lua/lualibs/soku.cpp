#include "../lualibs.hpp"
#include "../logger.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include "soku.hpp"
#include "gui.hpp"
#include "../script.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedPtr.h>
#include <sstream>
#include <shared_mutex>
#include <unordered_set>
#include <SokuLib.hpp>
#include <type_traits>

using namespace luabridge;

std::unordered_map<DWORD, std::unique_ptr<ShadyLua::Hook>> ShadyLua::hooks;

namespace {
    enum Event {
        /*RENDER,*/ PLAYERINFO, SCENECHANGE, READY, BATTLE,
        Count
    };
    struct eventHash { size_t operator()(const std::pair<int, ShadyLua::LuaScript*> &x) const { return x.first ^ (int)x.second; } };
    std::unordered_set<std::pair<int, ShadyLua::LuaScript*>, eventHash> eventMap[Event::Count];
    std::shared_mutex eventMapLock;

    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}

    struct charbuf : std::streambuf {
        charbuf(const char* begin, const char* end) {
            this->setg((char*)begin, (char*)begin, (char*)end);
        }
    };

    template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
    struct EnumStack : Stack<typename std::underlying_type<T>::type> {
        static inline T get(lua_State* L, int index) {
            return static_cast<T>(Stack<std::underlying_type<T>::type>::get(L, index));
        }
    };
}

// ---- ReadyHook ----
static bool __fastcall ReadyHook_replFn(void* unknown, int unused, void* data);
using ReadyHook = ShadyLua::CallHook<0x007fb871, ReadyHook_replFn>;
ReadyHook::typeFn ReadyHook::origFn = reinterpret_cast<ReadyHook::typeFn>(0x00407970);
static bool __fastcall ReadyHook_replFn(void* unknown, int unused, void* data) {
    auto ret = ReadyHook::origFn(unknown, unused, data);
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[Event::READY];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        if (lua_pcall(L, 0, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    return ret;
}

// ---- BattleHook ----
static void BattleHook_replFn(const SokuLib::GameStartParams& params);
using BattleHook = ShadyLua::CallHook<0x0043e63b, BattleHook_replFn>;
BattleHook::typeFn BattleHook::origFn = reinterpret_cast<BattleHook::typeFn>(0x004386a0);
static void BattleHook_replFn(const SokuLib::GameStartParams& params) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[Event::BATTLE];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        auto L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        luabridge::push(L, &params);
        if (lua_pcall(L, 1, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    return BattleHook::origFn(params);
}

// ---- PlayerInfoHook ----
static void __fastcall PlayerInfoHook_replFn(void* unknown, int unused, int index, const SokuLib::PlayerInfo& info);
using PlayerInfoHook = ShadyLua::CallHook<0x0046eb1b, PlayerInfoHook_replFn>;
PlayerInfoHook::typeFn PlayerInfoHook::origFn = reinterpret_cast<PlayerInfoHook::typeFn>(0x0046da40);
static void __fastcall PlayerInfoHook_replFn(void* unknown, int unused, int index, const SokuLib::PlayerInfo& info) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[Event::PLAYERINFO];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        auto L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        luabridge::push(L, &info);
        if (lua_pcall(L, 1, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    return PlayerInfoHook::origFn(unknown, unused, index, info);
}

// ---- SceneHook ----
namespace {
    std::unordered_map<int, int[6]> scene_hookedVT;

    static int __fastcall scene_onProcess(SokuLib::IScene* scene) {
        auto& vt = scene_hookedVT[*(int*)scene];
        int ret = reinterpret_cast<int(__fastcall*)(SokuLib::IScene*)>(vt[1])(scene);

        auto range = ShadyLua::SceneProxy::listeners.equal_range(scene);
        for (auto i = range.first; i != range.second; ++i) {
            int ret2 = i->second->onProcess();
            if (ret2 >= 0) ret = ret2;
        }
        return ret;
    }

    static bool __fastcall scene_onRender(SokuLib::IScene* scene) {
        auto& vt = scene_hookedVT[*(int*)scene];
        bool ret = reinterpret_cast<bool(__fastcall*)(SokuLib::IScene*)>(vt[2])(scene);

        auto range = ShadyLua::SceneProxy::listeners.equal_range(scene);
        for (auto i = range.first; i != range.second; ++i) {
            i->second->renderer.render();
        }
        return ret;
    }

    static void __fastcall scene_onLeave(SokuLib::IScene* scene, int unused, int sceneId) {
        auto& vt = scene_hookedVT[*(int*)scene];
        reinterpret_cast<void(__fastcall*)(SokuLib::IScene*, int, int)>(vt[5])(scene, unused, sceneId);

        auto range = ShadyLua::SceneProxy::listeners.equal_range(scene);
        for (auto i = range.first; i != range.second; ++i) {
            delete i->second;
        } ShadyLua::SceneProxy::listeners.erase(range.first, range.second);
    }

    static inline void _initSceneVTHooks(int* state, int* vtable) {
        DWORD prot; VirtualProtect((LPVOID)vtable, 4*6, PAGE_EXECUTE_WRITECOPY, &prot);
        std::memcpy(state, vtable, 4*6);
        vtable[1] = (int)scene_onProcess;
        vtable[2] = (int)scene_onRender;
        vtable[5] = (int)scene_onLeave;
        VirtualProtect((LPVOID)vtable, 4*6, prot, &prot);
    }

    static inline void initSceneVTHooks(SokuLib::IScene* scene) {
        auto iter = scene_hookedVT.find(*(int*)scene);
        if (iter == scene_hookedVT.end()) {
            _initSceneVTHooks(scene_hookedVT[*(int*)scene], *(int**)scene);
        }
    }
}

// ---- SceneHook (Change) ----
static SokuLib::IScene* __fastcall SceneHook_replFn(void* sceneManager, int unused, int sceneId);
using SceneHook = ShadyLua::AddressHook<0x00861af0, SceneHook_replFn>;
SceneHook::typeFn SceneHook::origFn = reinterpret_cast<SceneHook::typeFn>(0x0041e420);
static SokuLib::IScene* __fastcall SceneHook_replFn(void* sceneManager, int unused, int sceneId) {
    SokuLib::IScene* scene = SceneHook::origFn(sceneManager, unused, sceneId);

    { std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[Event::SCENECHANGE];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        auto L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        luabridge::push(L, sceneId);
        auto proxy = new ShadyLua::SceneProxy(L);
        luabridge::push(L, proxy);
        if (lua_pcall(L, 2, 1, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        } else if (lua_isfunction(L, -1)) {
            proxy->processHandler = luaL_ref(L, LUA_REGISTRYINDEX);
            ShadyLua::SceneProxy::listeners.emplace(scene, proxy);
            initSceneVTHooks(scene);
        } else if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
            ShadyLua::SceneProxy::listeners.emplace(scene, proxy);
            initSceneVTHooks(scene);
            lua_pop(L, 1);
        } else {
            delete proxy;
            lua_pop(L, 1);
        }
    } }

    return scene;
}

void ShadyLua::RemoveEvents(LuaScript* script) {
    RemoveLoaderEvents(script);
    RemoveBattleEvents(script);
    { std::unique_lock lock(eventMapLock);
    for (auto& hooks : eventMap) {
        auto i = hooks.begin(); while (i != hooks.end()) {
            if (i->second == script) i = hooks.erase(i);
            else ++i;
        }
    } }

    auto i = ShadyLua::SceneProxy::listeners.begin(); while(i != ShadyLua::SceneProxy::listeners.end()) {
        if (i->second->script == script) {
            delete i->second;
            i = ShadyLua::SceneProxy::listeners.erase(i);
        } else ++i;
    }
}

template<int eventType>
static int soku_SubscribeEvent(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);

    if constexpr(eventType == Event::READY) ShadyLua::initHook<ReadyHook>();
    if constexpr(eventType == Event::PLAYERINFO) ShadyLua::initHook<PlayerInfoHook>();
    if constexpr(eventType == Event::SCENECHANGE) ShadyLua::initHook<SceneHook>();
    if constexpr(eventType == Event::BATTLE) ShadyLua::initHook<BattleHook>();

    // iterators valid on insertion
    eventMap[eventType].insert(std::make_pair(callback, ShadyLua::ScriptMap[L]));
    if constexpr(eventType == Event::READY) if (*(bool*)(0x89ff90 + 0x4c)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        if (lua_pcall(L, 0, 0, 0)) { Logger::Error(lua_tostring(L, -1)); lua_pop(L, 1); } 
    }
    lua_pushnumber(L, callback);
    return 1;
}

static void soku_UnsubscribeEvent(int id, lua_State* L) {
    std::unique_lock guard(eventMapLock);
    luaL_unref(L, LUA_REGISTRYINDEX, id);
    for (auto& map : eventMap) map.erase(std::make_pair(id, ShadyLua::ScriptMap[L]));
}

static void soku_PlaySFX(int id) {
    reinterpret_cast<void (*)(int id)>(0x0043E1E0)(id);
}

static int soku_checkFKey(lua_State* L) {
    const int argc = lua_gettop(L);
    int key = luaL_checkinteger(L, 1);
    int m0 = (argc < 2) ? 0 : luaL_checkinteger(L, 2);
    int m1 = (argc < 3) ? 0 : luaL_checkinteger(L, 3);
    int m2 = (argc < 4) ? 0 : luaL_checkinteger(L, 4);
    luabridge::push(L, SokuLib::checkKeyOneshot(key, m0, m1, m2));
    return 1;
}

template<> struct luabridge::Stack<SokuLib::Character> : EnumStack<SokuLib::Character> {};
template<> struct luabridge::Stack<SokuLib::Stage> : EnumStack<SokuLib::Stage> {};

void ShadyLua::LualibSoku(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("soku")
            .beginNamespace("BattleEvent")
                .addConstant("GameStart",       0)
                .addConstant("RoundPreStart",   1)
                .addConstant("RoundStart",      2)
                .addConstant("RoundEnd",        3)
                .addConstant("GameEnd",         5)
                .addConstant("EndScreen",       6)
                .addConstant("ResultScreen",    7)
            .endNamespace()
            .beginNamespace("Character")
                .addConstant("Reimu",     SokuLib::Character::CHARACTER_REIMU)
                .addConstant("Marisa",    SokuLib::Character::CHARACTER_MARISA)
                .addConstant("Sakuya",    SokuLib::Character::CHARACTER_SAKUYA)
                .addConstant("Alice",     SokuLib::Character::CHARACTER_ALICE)
                .addConstant("Patchouli", SokuLib::Character::CHARACTER_PATCHOULI)
                .addConstant("Youmu",     SokuLib::Character::CHARACTER_YOUMU)
                .addConstant("Remilia",   SokuLib::Character::CHARACTER_REMILIA)
                .addConstant("Yuyuko",    SokuLib::Character::CHARACTER_YUYUKO)
                .addConstant("Yukari",    SokuLib::Character::CHARACTER_YUKARI)
                .addConstant("Suika",     SokuLib::Character::CHARACTER_SUIKA)
                .addConstant("Reisen",    SokuLib::Character::CHARACTER_REISEN)
                .addConstant("Aya",       SokuLib::Character::CHARACTER_AYA)
                .addConstant("Komachi",   SokuLib::Character::CHARACTER_KOMACHI)
                .addConstant("Iku",       SokuLib::Character::CHARACTER_IKU)
                .addConstant("Tenshi",    SokuLib::Character::CHARACTER_TENSHI)
                .addConstant("Sanae",     SokuLib::Character::CHARACTER_SANAE)
                .addConstant("Cirno",     SokuLib::Character::CHARACTER_CIRNO)
                .addConstant("Meiling",   SokuLib::Character::CHARACTER_MEILING)
                .addConstant("Utsuho",    SokuLib::Character::CHARACTER_UTSUHO)
                .addConstant("Suwako",    SokuLib::Character::CHARACTER_SUWAKO)
                .addConstant("Random",    SokuLib::Character::CHARACTER_RANDOM)
                .addConstant("Namazu",    SokuLib::Character::CHARACTER_NAMAZU)
            .endNamespace()
            .beginNamespace("Scene")
                .addConstant("Logo",            (int)SokuLib::Scene::SCENE_LOGO)
                .addConstant("Opening",         (int)SokuLib::Scene::SCENE_OPENING)
                .addConstant("Title",           (int)SokuLib::Scene::SCENE_TITLE)
                .addConstant("Select",          (int)SokuLib::Scene::SCENE_SELECT)
                .addConstant("Battle",          (int)SokuLib::Scene::SCENE_BATTLE)
                .addConstant("Loading",         (int)SokuLib::Scene::SCENE_LOADING)
                .addConstant("SelectSV",        (int)SokuLib::Scene::SCENE_SELECTSV)
                .addConstant("SelectCL",        (int)SokuLib::Scene::SCENE_SELECTCL)
                .addConstant("LoadingSV",       (int)SokuLib::Scene::SCENE_LOADINGSV)
                .addConstant("LoadingCL",       (int)SokuLib::Scene::SCENE_LOADINGCL)
                .addConstant("LoadingWatch",    (int)SokuLib::Scene::SCENE_LOADINGWATCH)
                .addConstant("BattleSV",        (int)SokuLib::Scene::SCENE_BATTLESV)
                .addConstant("BattleCL",        (int)SokuLib::Scene::SCENE_BATTLECL)
                .addConstant("BattleWatch",     (int)SokuLib::Scene::SCENE_BATTLEWATCH)
                .addConstant("SelectStage",     (int)SokuLib::Scene::SCENE_SELECTSCENARIO)
                .addConstant("Ending",          (int)SokuLib::Scene::SCENE_ENDING)
                //.addVariable("Submenu",         21)
            .endNamespace()
            .beginNamespace("Stage")
                .addConstant("HakureiShrine",           SokuLib::Stage::STAGE_HAKUREI_SHRINE_BROKEN)
                .addConstant("ForestOfMagic",           SokuLib::Stage::STAGE_FOREST_OF_MAGIC)
                .addConstant("CreekOfGenbu",            SokuLib::Stage::STAGE_CREEK_OF_GENBU)
                .addConstant("YoukaiMountain",          SokuLib::Stage::STAGE_YOUKAI_MOUNTAIN)
                .addConstant("MysteriousSeaOfClouds",   SokuLib::Stage::STAGE_MYSTERIOUS_SEA_OF_CLOUD)
                .addConstant("BhavaAgra",               SokuLib::Stage::STAGE_BHAVA_AGRA)
                .addConstant("Space",                   SokuLib::Stage::STAGE_SPACE)
                .addConstant("RepairedHakureiShrine",   SokuLib::Stage::STAGE_HAKUREI_SHRINE)
                .addConstant("KirisameMagicShop",       SokuLib::Stage::STAGE_KIRISAME_MAGIC_SHOP)
                .addConstant("SDMClockTower",           SokuLib::Stage::STAGE_SCARLET_DEVIL_MANSION_CLOCK_TOWER)
                .addConstant("ForestOfDolls",           SokuLib::Stage::STAGE_FOREST_OF_DOLLS)
                .addConstant("SDMLibrary",              SokuLib::Stage::STAGE_SCARLET_DEVIL_MANSION_LIBRARY)
                .addConstant("Netherworld",             SokuLib::Stage::STAGE_NETHERWORLD)
                .addConstant("SDMFoyer",                SokuLib::Stage::STAGE_SCARLET_DEVIL_MANSION_FOYER)
                .addConstant("HakugyokurouSnowyGarden", SokuLib::Stage::STAGE_HAKUGYOKUROU_SNOWY_GARDEN)
                .addConstant("BambooForestOfTheLost",   SokuLib::Stage::STAGE_BAMBOO_FOREST_OF_THE_LOST)
                .addConstant("ShoreOfMistyLake",        SokuLib::Stage::STAGE_SHORE_OF_MISTY_LAKE)
                .addConstant("MoriyaShrine",            SokuLib::Stage::STAGE_MORIYA_SHRINE)
                .addConstant("MouthOfGeyser",           SokuLib::Stage::STAGE_MOUTH_OF_GEYSER)
                .addConstant("CatwalkInGeyser",         SokuLib::Stage::STAGE_CATWALK_OF_GEYSER)
                .addConstant("FusionReactorCore",       SokuLib::Stage::STAGE_FUSION_REACTOR_CORE)
                .addConstant("SDMClockTowerBG",         SokuLib::Stage::STAGE_SCARLET_DEVIL_MANSION_CLOCK_TOWER_SKETCH_BG)
                .addConstant("SDMClockTowerBlurry",     SokuLib::Stage::STAGE_SCARLET_DEVIL_MANSION_CLOCK_TOWER_BLURRY)
                .addConstant("SDMClockTowerSketch",     SokuLib::Stage::STAGE_SCARLET_DEVIL_MANSION_CLOCK_TOWER_SKETCH)
            .endNamespace()
            .addProperty("isReady", (bool*)(0x89ff90 + 0x4c), false)
            .addVariable("P1", &SokuLib::leftPlayerInfo)
            .addVariable("P2", &SokuLib::rightPlayerInfo)
            .addVariable("sceneId", &SokuLib::sceneId, false)
            .addFunction("checkFKey", soku_checkFKey)
            .addFunction("playSE", soku_PlaySFX)
            .addFunction("playSFX", soku_PlaySFX)

            .addCFunction("SubscribePlayerInfo", soku_SubscribeEvent<Event::PLAYERINFO>)
            .addCFunction("SubscribeSceneChange", soku_SubscribeEvent<Event::SCENECHANGE>)
            .addCFunction("SubscribeReady", soku_SubscribeEvent<Event::READY>)
            .addCFunction("SubscribeBattle", soku_SubscribeEvent<Event::BATTLE>)
            .addFunction("UnsubscribeEvent", soku_UnsubscribeEvent)
            .beginClass<SokuLib::Vector2f>("Vector2f")
                .addData("x", &SokuLib::Vector2f::x, true)
                .addData("y", &SokuLib::Vector2f::y, true)
            .endClass()
        .endNamespace()
    ;
}