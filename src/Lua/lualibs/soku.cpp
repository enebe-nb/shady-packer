#include "../lualibs.hpp"
#include "../logger.hpp"
#include "../strconv.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include "soku.hpp"
#include "../script.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedPtr.h>
#include <sstream>
#include <shared_mutex>
#include <unordered_map>

using namespace luabridge;

namespace {
    std::unordered_map<int, std::unordered_map<int, ShadyLua::LuaScript*> > eventMap;
    std::shared_mutex eventMapLock;
    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}
}

/*
static int soku_Player_Character(const Soku::CPlayer* player) {
    return (int)player->CharID();
}

static void soku_Player_PlaySE(const Soku::CPlayer* player, int id) {
    const DWORD addr = player->ADDR();
    __asm mov ecx, addr;
    reinterpret_cast<void(WINAPI*)(int)>(0x00464980)(id);
}

static void soku_ReloadSE(int id) {
    char buffer[] = "data/se/000.wav";
    sprintf(buffer, "data/se/%03d.wav", id);
    __asm mov ecx, 0x0089f9f8;
    reinterpret_cast<void(WINAPI*)(int*, char*)>(0x00401af0)((int*)(0x00899D60 + id*4), buffer);
}
*/

const int soku_EventRender = 0;
const int soku_EventBattleEvent = 3;
//const int soku_EventGameEvent = 4;
const int soku_EventStageSelect = 5;
const int soku_EventFileLoader = 6;

void ShadyLua::EmitSokuEventRender() {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventRender];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
    //     lua_pushinteger(L, (int)data.scene);
    //     lua_pushinteger(L, (int)data.mode);
    //     lua_pushinteger(L, (int)data.menu);
        if (lua_pcall(L, 0, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void ShadyLua::EmitSokuEventBattleEvent(int eventId) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventBattleEvent];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        lua_pushinteger(L, (int)eventId);
        if (lua_pcall(L, 1, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

// void ShadyLua::EmitSokuEventGameEvent(int sceneId) {
//     std::shared_lock guard(eventMapLock);
//     auto& listeners = eventMap[soku_EventGameEvent];
//     for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
//         std::lock_guard scriptGuard(iter->second->mutex);

//         lua_State* L = iter->second->L;
//         lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
//         lua_pushinteger(L, sceneId);
//         if (lua_pcall(L, 1, 0, 0)) {
//             Logger::Error(lua_tostring(L, -1));
//             lua_pop(L, 1);
//         }
//     }
// }

void ShadyLua::EmitSokuEventStageSelect(int* stageId) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventStageSelect];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        lua_pushinteger(L, *stageId);
        if (lua_pcall(L, 1, 1, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        } switch(lua_type(L, 1)) {
            case LUA_TNUMBER:
                *stageId = lua_tonumber(L, 1);
        } lua_pop(L, 1);
    }
}

namespace {
    struct charbuf : std::streambuf {
        charbuf(const char* begin, const char* end) {
            this->setg((char*)begin, (char*)begin, (char*)end);
        }
    };
}

void ShadyLua::EmitSokuEventFileLoader(const char* filename, std::istream** input, int* size) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventFileLoader];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        lua_pushstring(L, filename);
        if (lua_pcall(L, 1, 2, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        } switch(lua_type(L, 1)) {
            case LUA_TSTRING: {
                size_t dataSize = 0;
                const char* data = lua_tolstring(L, 1, &dataSize);
                if (!data || !dataSize) break;
                const char* format = lua_tostring(L, 2);
                if (strcmp(format, "png") == 0) {
                    charbuf decBuf(data, data + dataSize);
                    std::istream decStream(&decBuf);
                    std::stringstream* encStream = 
                        new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
                    ShadyCore::convertResource(ShadyCore::FileType::TYPE_IMAGE,
                        ShadyCore::FileType::IMAGE_PNG, decStream,
                        ShadyCore::FileType::IMAGE_GAME, *encStream);
                    *size = encStream->tellp();
                    *input = encStream;
                    lua_pop(L, 2);
                    return;
                }
                *size = dataSize;
                *input = new std::stringstream(std::string(data, dataSize),
                    std::ios::in|std::ios::out|std::ios::binary);
                lua_pop(L, 2);
                return;
            }
            case LUA_TUSERDATA: {
                RefCountedPtr<ShadyCore::Resource> resource = Stack<ShadyCore::Resource*>::get(L, 1);
                std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
                throw std::exception("TODO fix");
                //ShadyCore::writeResource(resource.get(), *buffer, true);
                *size = buffer->tellp();
                *input = buffer;
                lua_pop(L, 2);
                return;
            }break;
        }
        lua_pop(L, 2);
    }
}

static int soku_SubscribeEvent(lua_State* L) {
    int eventType = luaL_checkinteger(L, 1);
    if (lua_gettop(L) < 2 || !lua_isfunction(L, 2)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    // iterators valid on insertion
    eventMap[eventType][callback] = ShadyLua::ScriptMap[L];
    lua_pushnumber(L, callback);
    return 1;
}

template<int eventType>
static int soku_SubscribeEventTyped(lua_State* L) {
    lua_pushinteger(L, eventType);
    lua_insert(L, 1);
    return soku_SubscribeEvent(L);
}

static void soku_UnsubscribeEvent(int id, lua_State* L) {
    std::unique_lock guard(eventMapLock);
    luaL_unref(L, LUA_REGISTRYINDEX, id);
    eventMap.erase(id);
}

static void soku_PlaySE(int id) {
    reinterpret_cast<void (*)(int id)>(0x0043E1E0)(id);
}

const int soku_CharacterReimu     = 0;
const int soku_CharacterMarisa    = 1;
const int soku_CharacterSakuya    = 2;
const int soku_CharacterAlice     = 3;
const int soku_CharacterPatchouli = 4;
const int soku_CharacterYoumu     = 5;
const int soku_CharacterRemilia   = 6;
const int soku_CharacterYuyuko    = 7;
const int soku_CharacterYukari    = 8;
const int soku_CharacterSuika     = 9;
const int soku_CharacterReisen    = 10;
const int soku_CharacterAya       = 11;
const int soku_CharacterKomachi   = 12;
const int soku_CharacterIku       = 13;
const int soku_CharacterTenshi    = 14;
const int soku_CharacterSanae     = 15;
const int soku_CharacterCirno     = 16;
const int soku_CharacterMeiling   = 17;
const int soku_CharacterUtsuho    = 18;
const int soku_CharacterSuwako    = 19;
const int soku_CharacterRandom    = 20;
const int soku_CharacterNamazu    = 21;

const int soku_StageHakureiShrine           = 0;
const int soku_StageForestOfMagic           = 1;
const int soku_StageCreekOfGenbu            = 2;
const int soku_StageYoukaiMountain          = 3;
const int soku_StageMysteriousSeaOfClouds   = 4;
const int soku_StageBhavaAgra               = 5;
const int soku_StageRepairedHakureiShrine   = 10;
const int soku_StageKirisameMagicShop       = 11;
const int soku_StageSDMClockTower           = 12;
const int soku_StageForestOfDolls           = 13;
const int soku_StageSDMLibrary              = 14;
const int soku_StageNetherworld             = 15;
const int soku_StageSDMFoyer                = 16;
const int soku_StageHakugyokurouSnowyGarden = 17;
const int soku_StageBambooForestOfTheLost   = 18;
const int soku_StageShoreOfMistyLake        = 30;
const int soku_StageMoriyaShrine            = 31;
const int soku_StageMouthOfGeyser           = 32;
const int soku_StageCatwalkInGeyser         = 33;
const int soku_StageFusionReactorCore       = 34;

const int soku_SceneLogo            = 0;
const int soku_SceneOpening         = 1;
const int soku_SceneTitle           = 2;
const int soku_SceneSelect          = 3;
const int soku_SceneBattle          = 5;
const int soku_SceneLoading         = 6;
const int soku_SceneSelectSv        = 8;
const int soku_SceneSelectCl        = 9;
const int soku_SceneLoadingSv       = 10;
const int soku_SceneLoadingCl       = 11;
const int soku_SceneLoadingWatch    = 12;
const int soku_SceneBattleSv        = 13;
const int soku_SceneBattleCl        = 14;
const int soku_SceneBattleWatch     = 15;
const int soku_SceneSelectStage     = 16;
const int soku_SceneEnding          = 20;
const int soku_SceneSubmenu         = 21;

const int soku_BattleGameStart      = 0;
const int soku_BattleRoundPreStart  = 1;
const int soku_BattleRoundStart     = 2;
const int soku_BattleRoundEnd       = 3;
const int soku_BattleGameEnd        = 5;
const int soku_BattleEndScreen      = 6;
const int soku_BattleResultScreen   = 7;

void ShadyLua::LualibSoku(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("soku")
            .beginNamespace("Event")
                .addVariable("Render", (int*)&soku_EventRender, false)
                .addVariable("BattleEvent", (int*)&soku_EventBattleEvent, false)
                //.addVariable("GameEvent", (int*)&soku_EventGameEvent, false)
                .addVariable("StageSelect", (int*)&soku_EventStageSelect, false)
                .addVariable("FileLoader", (int*)&soku_EventFileLoader, false)
            .endNamespace()
            .beginNamespace("BattleEvent")
                .addVariable("GameStart",       (int*)&soku_BattleGameStart,        false)
                .addVariable("RoundPreStart",   (int*)&soku_BattleRoundPreStart,    false)
                .addVariable("RoundStart",      (int*)&soku_BattleRoundStart,       false)
                .addVariable("RoundEnd",        (int*)&soku_BattleRoundEnd,         false)
                .addVariable("GameEnd",         (int*)&soku_BattleGameEnd,          false)
                .addVariable("EndScreen",       (int*)&soku_BattleEndScreen,        false)
                .addVariable("ResultScreen",    (int*)&soku_BattleResultScreen,     false)
            .endNamespace()
            .beginNamespace("Character")
                .addVariable("Reimu",     (int*)&soku_CharacterReimu,     false)
                .addVariable("Marisa",    (int*)&soku_CharacterMarisa,    false)
                .addVariable("Sakuya",    (int*)&soku_CharacterSakuya,    false)
                .addVariable("Alice",     (int*)&soku_CharacterAlice,     false)
                .addVariable("Patchouli", (int*)&soku_CharacterPatchouli, false)
                .addVariable("Youmu",     (int*)&soku_CharacterYoumu,     false)
                .addVariable("Remilia",   (int*)&soku_CharacterRemilia,   false)
                .addVariable("Yuyuko",    (int*)&soku_CharacterYuyuko,    false)
                .addVariable("Yukari",    (int*)&soku_CharacterYukari,    false)
                .addVariable("Suika",     (int*)&soku_CharacterSuika,     false)
                .addVariable("Reisen",    (int*)&soku_CharacterReisen,    false)
                .addVariable("Aya",       (int*)&soku_CharacterAya,       false)
                .addVariable("Komachi",   (int*)&soku_CharacterKomachi,   false)
                .addVariable("Iku",       (int*)&soku_CharacterIku,       false)
                .addVariable("Tenshi",    (int*)&soku_CharacterTenshi,    false)
                .addVariable("Sanae",     (int*)&soku_CharacterSanae,     false)
                .addVariable("Cirno",     (int*)&soku_CharacterCirno,     false)
                .addVariable("Meiling",   (int*)&soku_CharacterMeiling,   false)
                .addVariable("Utsuho",    (int*)&soku_CharacterUtsuho,    false)
                .addVariable("Suwako",    (int*)&soku_CharacterSuwako,    false)
                .addVariable("Random",    (int*)&soku_CharacterRandom,    false)
                .addVariable("Namazu",    (int*)&soku_CharacterNamazu,    false)
            .endNamespace()
            /*
            .beginNamespace("GameEvent")
                .addVariable("ServerSetup", enumMap<(int)GameEvent::SERVER_SETUP>(), false)
                .addVariable("ServerRelease", enumMap<(int)GameEvent::SERVER_RELEASE>(), false)
                .addVariable("ConnectToServer", enumMap<(int)GameEvent::CONNECT_TO_SERVER>(), false)
                .addVariable("TitleCreate", enumMap<(int)GameEvent::TITLE_CREATE>(), false)
                .addVariable("TitleProcess", enumMap<(int)GameEvent::TITLE_PROCESS>(), false)
                .addVariable("BattleManagerCreate", enumMap<(int)GameEvent::BATTLE_MANAGER_CERATE>(), false)
                .addVariable("BattleManagerClear", enumMap<(int)GameEvent::BATTLE_MANAGER_CLEAR>(), false)
                .addVariable("SubmenuProcess", enumMap<(int)GameEvent::SUBMENU_PROCESS>(), false)
                .addVariable("SubmenuRelease", enumMap<(int)GameEvent::SUBMENU_RELEASE>(), false)
                .addVariable("StoryCreate", enumMap<(int)GameEvent::STORY_CREATE>(), false)
                .addVariable("ArcadeCreate", enumMap<(int)GameEvent::ARCADE_CREATE>(), false)
                .addVariable("VsComCreate", enumMap<(int)GameEvent::VS_COM_CREATE>(), false)
                .addVariable("VsPlayerCreate", enumMap<(int)GameEvent::VS_PLAYER_CREATE>(), false)
                .addVariable("VsClientCreate", enumMap<(int)GameEvent::VS_CLIENT_CREATE>(), false)
                .addVariable("VsServerCreate", enumMap<(int)GameEvent::VS_SERVER_CREATE>(), false)
                .addVariable("WatchCreate", enumMap<(int)GameEvent::WATCH_CREATE>(), false)
                .addVariable("PracticeCreate", enumMap<(int)GameEvent::PRACTICE_CREATE>(), false)
                .addVariable("ReplayCreate", enumMap<(int)GameEvent::REPLAY_CREATE>(), false)
                .addVariable("ResultCreate", enumMap<(int)GameEvent::RESULT_CREATE>(), false)
                .addVariable("Unknown", enumMap<(int)GameEvent::UNKNOWN>(), false)
            .endNamespace()
            .beginNamespace("Menu")
                .addVariable("Custom", enumMap<(int)MENU::CUSTOM>(), false)
                .addVariable("VsNetwork", enumMap<(int)MENU::VS_NETWORK>(), false)
                .addVariable("Replay", enumMap<(int)MENU::REPLAY>(), false)
                .addVariable("MusicRoom", enumMap<(int)MENU::MUSICROOM>(), false)
                .addVariable("Result", enumMap<(int)MENU::RESULT>(), false)
                .addVariable("Profile", enumMap<(int)MENU::PROFILE>(), false)
                .addVariable("Profile2", enumMap<(int)MENU::PROFILE_V2>(), false)
                .addVariable("Config", enumMap<(int)MENU::CONFIG>(), false)
                .addVariable("Practice", enumMap<(int)MENU::PRACTICE>(), false)
                .addVariable("Pause", enumMap<(int)MENU::PAUSE>(), false)
                .addVariable("None", enumMap<(int)MENU::NONE>(), false)
                .addVariable("Unknown", enumMap<(int)MENU::UNKNOWN>(), false)
            .endNamespace()
            .beginNamespace("Mode")
                .addVariable("Story", enumMap<(int)MODE::STORY>(), false)
                .addVariable("Arcade", enumMap<(int)MODE::ARCADE>(), false)
                .addVariable("VsCom", enumMap<(int)MODE::VSCOM>(), false)
                .addVariable("VsPlayer", enumMap<(int)MODE::VSPLAYER>(), false)
                .addVariable("VsClient", enumMap<(int)MODE::VSCLIENT>(), false)
                .addVariable("VsServer", enumMap<(int)MODE::VSSERVER>(), false)
                .addVariable("VsWatch", enumMap<(int)MODE::VSWATCH>(), false)
                .addVariable("Practice", enumMap<(int)MODE::PRACTICE>(), false)
                .addVariable("Replay", enumMap<(int)MODE::REPLAY>(), false)
                .addVariable("None", enumMap<(int)MODE::NONE>(), false)
            .endNamespace()
            */
            .beginNamespace("Scene")
                .addVariable("Logo",            (int*)&soku_SceneLogo,          false)
                .addVariable("Opening",         (int*)&soku_SceneOpening,       false)
                .addVariable("Title",           (int*)&soku_SceneTitle,         false)
                .addVariable("Select",          (int*)&soku_SceneSelect,        false)
                .addVariable("Battle",          (int*)&soku_SceneBattle,        false)
                .addVariable("SelectSV",        (int*)&soku_SceneSelectSv,      false)
                .addVariable("SelectCL",        (int*)&soku_SceneSelectCl,      false)
                .addVariable("LoadingSV",       (int*)&soku_SceneLoadingSv,     false)
                .addVariable("LoadingCL",       (int*)&soku_SceneLoadingCl,     false)
                .addVariable("LoadingWatch",    (int*)&soku_SceneLoadingWatch,  false)
                .addVariable("BattleSV",        (int*)&soku_SceneBattleSv,      false)
                .addVariable("BattleCL",        (int*)&soku_SceneBattleCl,      false)
                .addVariable("BattleWatch",     (int*)&soku_SceneBattleWatch,   false)
                .addVariable("SelectStage",     (int*)&soku_SceneSelectStage,   false)
                .addVariable("Ending",          (int*)&soku_SceneEnding,        false)
                .addVariable("Submenu",         (int*)&soku_SceneSubmenu,       false)
            .endNamespace()
            .beginNamespace("Stage")
                .addVariable("HakureiShrine",           (int*)&soku_StageHakureiShrine,           false)
                .addVariable("ForestOfMagic",           (int*)&soku_StageForestOfMagic,           false)
                .addVariable("CreekOfGenbu",            (int*)&soku_StageCreekOfGenbu,            false)
                .addVariable("YoukaiMountain",          (int*)&soku_StageYoukaiMountain,          false)
                .addVariable("MysteriousSeaOfClouds",   (int*)&soku_StageMysteriousSeaOfClouds,   false)
                .addVariable("BhavaAgra",               (int*)&soku_StageBhavaAgra,               false)
                .addVariable("RepairedHakureiShrine",   (int*)&soku_StageRepairedHakureiShrine,   false)
                .addVariable("KirisameMagicShop",       (int*)&soku_StageKirisameMagicShop,       false)
                .addVariable("SDMClockTower",           (int*)&soku_StageSDMClockTower,           false)
                .addVariable("ForestOfDolls",           (int*)&soku_StageForestOfDolls,           false)
                .addVariable("SDMLibrary",              (int*)&soku_StageSDMLibrary,              false)
                .addVariable("Netherworld",             (int*)&soku_StageNetherworld,             false)
                .addVariable("SDMFoyer",                (int*)&soku_StageSDMFoyer,                false)
                .addVariable("HakugyokurouSnowyGarden", (int*)&soku_StageHakugyokurouSnowyGarden, false)
                .addVariable("BambooForestOfTheLost",   (int*)&soku_StageBambooForestOfTheLost,   false)
                .addVariable("ShoreOfMistyLake",        (int*)&soku_StageShoreOfMistyLake,        false)
                .addVariable("MoriyaShrine",            (int*)&soku_StageMoriyaShrine,            false)
                .addVariable("MouthOfGeyser",           (int*)&soku_StageMouthOfGeyser,           false)
                .addVariable("CatwalkInGeyser",         (int*)&soku_StageCatwalkInGeyser,         false)
                .addVariable("FusionReactorCore",       (int*)&soku_StageFusionReactorCore,       false)
            .endNamespace()
            //.beginClass<Soku::CPlayer>("Player")
                //.addProperty("Character", soku_Player_Character)
                //.addFunction("PlaySE", soku_Player_PlaySE)
            //.endClass()
            //.addVariable("P1", &Soku::P1)
            //.addVariable("P2", &Soku::P2)
            .addFunction("PlaySE", soku_PlaySE)
            //.addFunction("ReloadSE", soku_ReloadSE)
            .addCFunction("SubscribeEvent", soku_SubscribeEvent)
            .addCFunction("SubscribeRender", soku_SubscribeEventTyped<soku_EventRender>)
            .addCFunction("SubscribeBattleEvent", soku_SubscribeEventTyped<soku_EventBattleEvent>)
            //.addCFunction("SubscribeGameEvent", soku_SubscribeEventTyped<soku_EventGameEvent>)
            .addCFunction("SubscribeStageSelect", soku_SubscribeEventTyped<soku_EventStageSelect>)
            .addCFunction("SubscribeFileLoader", soku_SubscribeEventTyped<soku_EventFileLoader>)
            .addFunction("UnsubscribeEvent", soku_UnsubscribeEvent)
        .endNamespace()
    ;
}