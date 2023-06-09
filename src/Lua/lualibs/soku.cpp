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

std::unique_ptr<ShadyLua::Hook> ShadyLua::hooks[Hook::Type::Count];

namespace {
    enum Event {
        /*RENDER,*/ PLAYERINFO, SCENECHANGE, READY,
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
using ReadyHook = ShadyLua::CallHook<0x007fb871, ShadyLua::Hook::Type::READY, ReadyHook_replFn>;
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

// ---- PlayerInfoHook ----
static void __fastcall PlayerInfoHook_replFn(void* unknown, int unused, int index, const SokuLib::PlayerInfo& info);
using PlayerInfoHook = ShadyLua::CallHook<0x0046eb1b, ShadyLua::Hook::Type::PLAYER_INFO, PlayerInfoHook_replFn>;
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
        reinterpret_cast<void(__fastcall*)(SokuLib::IScene*, int, int)>(vt[2])(scene, unused, sceneId);

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
using SceneHook = ShadyLua::AddressHook<0x00861af0, ShadyLua::Hook::Type::SCENE_CHANGE, SceneHook_replFn>;
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
    std::unique_lock lock(eventMapLock);
    for (auto& hooks : eventMap) {
        auto i = hooks.begin(); while (i != hooks.end()) {
            if (i->second == script) i = hooks.erase(i);
            else ++i;
        }
    }
}

static void soku_Player_PlaySE(const SokuLib::PlayerInfo* player, int id) {
    constexpr int callAddr = 0x00464980;
    __asm {
        mov ecx, id;
        push ecx;
        mov ecx, player;
        call callAddr;
    }
}

template<int eventType>
static int soku_SubscribeEvent(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);

    if constexpr(eventType == Event::READY) ShadyLua::initHook<ReadyHook>();
    if constexpr(eventType == Event::PLAYERINFO) ShadyLua::initHook<PlayerInfoHook>();
    if constexpr(eventType == Event::SCENECHANGE) ShadyLua::initHook<SceneHook>();

    // iterators valid on insertion
    eventMap[eventType].insert(std::make_pair(callback, ShadyLua::ScriptMap[L]));
    lua_pushnumber(L, callback);
    return 1;
}

static void soku_UnsubscribeEvent(int id, lua_State* L) {
    std::unique_lock guard(eventMapLock);
    luaL_unref(L, LUA_REGISTRYINDEX, id);
    for (auto& map : eventMap) map.erase(std::make_pair(id, ShadyLua::ScriptMap[L]));
}

static void soku_PlaySE(int id) {
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
                .addVariable("GameStart",       enumMap<0>(), false)
                .addVariable("RoundPreStart",   enumMap<1>(), false)
                .addVariable("RoundStart",      enumMap<2>(), false)
                .addVariable("RoundEnd",        enumMap<3>(), false)
                .addVariable("GameEnd",         enumMap<5>(), false)
                .addVariable("EndScreen",       enumMap<6>(), false)
                .addVariable("ResultScreen",    enumMap<7>(), false)
            .endNamespace()
            .beginNamespace("Character")
                .addVariable("Reimu",     enumMap<0>(), false)
                .addVariable("Marisa",    enumMap<1>(), false)
                .addVariable("Sakuya",    enumMap<2>(), false)
                .addVariable("Alice",     enumMap<3>(), false)
                .addVariable("Patchouli", enumMap<4>(), false)
                .addVariable("Youmu",     enumMap<5>(), false)
                .addVariable("Remilia",   enumMap<6>(), false)
                .addVariable("Yuyuko",    enumMap<7>(), false)
                .addVariable("Yukari",    enumMap<8>(), false)
                .addVariable("Suika",     enumMap<9>(), false)
                .addVariable("Reisen",    enumMap<10>(), false)
                .addVariable("Aya",       enumMap<11>(), false)
                .addVariable("Komachi",   enumMap<12>(), false)
                .addVariable("Iku",       enumMap<13>(), false)
                .addVariable("Tenshi",    enumMap<14>(), false)
                .addVariable("Sanae",     enumMap<15>(), false)
                .addVariable("Cirno",     enumMap<16>(), false)
                .addVariable("Meiling",   enumMap<17>(), false)
                .addVariable("Utsuho",    enumMap<18>(), false)
                .addVariable("Suwako",    enumMap<19>(), false)
                .addVariable("Random",    enumMap<20>(), false)
                .addVariable("Namazu",    enumMap<21>(), false)
            .endNamespace()
            .beginNamespace("Scene")
                .addVariable("Logo",            enumMap<0>(), false)
                .addVariable("Opening",         enumMap<1>(), false)
                .addVariable("Title",           enumMap<2>(), false)
                .addVariable("Select",          enumMap<3>(), false)
                .addVariable("Battle",          enumMap<5>(), false)
                .addVariable("SelectSV",        enumMap<8>(), false)
                .addVariable("SelectCL",        enumMap<9>(), false)
                .addVariable("LoadingSV",       enumMap<10>(), false)
                .addVariable("LoadingCL",       enumMap<11>(), false)
                .addVariable("LoadingWatch",    enumMap<12>(), false)
                .addVariable("BattleSV",        enumMap<13>(), false)
                .addVariable("BattleCL",        enumMap<14>(), false)
                .addVariable("BattleWatch",     enumMap<15>(), false)
                .addVariable("SelectStage",     enumMap<16>(), false)
                .addVariable("Ending",          enumMap<20>(), false)
                .addVariable("Submenu",         enumMap<21>(), false)
            .endNamespace()
            .beginNamespace("Stage")
                .addVariable("HakureiShrine",           enumMap<0>(), false)
                .addVariable("ForestOfMagic",           enumMap<1>(), false)
                .addVariable("CreekOfGenbu",            enumMap<2>(), false)
                .addVariable("YoukaiMountain",          enumMap<3>(), false)
                .addVariable("MysteriousSeaOfClouds",   enumMap<4>(), false)
                .addVariable("BhavaAgra",               enumMap<5>(), false)
                .addVariable("RepairedHakureiShrine",   enumMap<10>(), false)
                .addVariable("KirisameMagicShop",       enumMap<11>(), false)
                .addVariable("SDMClockTower",           enumMap<12>(), false)
                .addVariable("ForestOfDolls",           enumMap<13>(), false)
                .addVariable("SDMLibrary",              enumMap<14>(), false)
                .addVariable("Netherworld",             enumMap<15>(), false)
                .addVariable("SDMFoyer",                enumMap<16>(), false)
                .addVariable("HakugyokurouSnowyGarden", enumMap<17>(), false)
                .addVariable("BambooForestOfTheLost",   enumMap<18>(), false)
                .addVariable("ShoreOfMistyLake",        enumMap<30>(), false)
                .addVariable("MoriyaShrine",            enumMap<31>(), false)
                .addVariable("MouthOfGeyser",           enumMap<32>(), false)
                .addVariable("CatwalkInGeyser",         enumMap<33>(), false)
                .addVariable("FusionReactorCore",       enumMap<34>(), false)
            .endNamespace()
            .beginClass<SokuLib::PlayerInfo>("PlayerInfo")
                .addProperty("character", &SokuLib::PlayerInfo::character, false)
                .addProperty("isRight", &SokuLib::PlayerInfo::isRight, false)
                .addProperty("palette", &SokuLib::PlayerInfo::palette, false)
                .addProperty("deck", &SokuLib::PlayerInfo::deck, false)
                // .addFunction("PlaySE", soku_Player_PlaySE) // TODO something does not match with this
            .endClass()
            .addProperty("IsReady", (bool*)(0x89ff90 + 0x4c), false)
            .addVariable("P1", &SokuLib::leftPlayerInfo) // TODO something does not match with this
            .addVariable("P2", &SokuLib::rightPlayerInfo) // TODO something does not match with this
            .addVariable("SceneId", &SokuLib::sceneId, false)
            .addFunction("CheckFKey", soku_checkFKey)
            .addFunction("PlaySE", soku_PlaySE)
            .addCFunction("SubscribePlayerInfo", soku_SubscribeEvent<Event::PLAYERINFO>)
            .addCFunction("SubscribeSceneChange", soku_SubscribeEvent<Event::SCENECHANGE>)
            .addCFunction("SubscribeReady", soku_SubscribeEvent<Event::READY>)
            .addFunction("UnsubscribeEvent", soku_UnsubscribeEvent)
            .beginClass<SokuLib::Vector2f>("Vector2f")
                .addConstructor<void(*)(void)>()
                .addData("x", &SokuLib::Vector2f::x, true)
                .addData("y", &SokuLib::Vector2f::y, true)
            .endClass()
        .endNamespace()
    ;
}