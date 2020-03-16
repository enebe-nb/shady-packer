#include "../lualibs.hpp"
#include "../logger.hpp"
#include "../strconv.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include "../../Loader/reader.hpp"
#include <SokuLib.h>
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedPtr.h>
#include <sstream>
#include <algorithm>
#include <unordered_map>

using namespace luabridge;

namespace {
    std::unordered_map<int, EventID> subscribeMap;
}

template <int value>
static inline int* enumMap()
    {static const int valueHolder = value; return (int*)&valueHolder;}

static bool soku_Module(const char* name) {
    std::wstring wname(s2ws(name));
    for (auto& m : Soku::GetModuleList()) {
        if (m.Name() == wname) {
            if (!m.IsInjected()) m.Inject();
            return true;
        }
    } return false;
}

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

static int soku_SubscribeRender(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::Render, [L, callback](SokuData::RenderData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushinteger(L, (int)data.scene);
        lua_pushinteger(L, (int)data.mode);
        lua_pushinteger(L, (int)data.menu);
        if (lua_pcall(L, 3, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeWindowProc(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::WindowProc, [L, callback](SokuData::WindowProcData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushinteger(L, data.uMsg);
        lua_pushinteger(L, data.wParam);
        lua_pushinteger(L, data.lParam);
        if (lua_pcall(L, 3, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeKeyboard(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::Keyboard, [L, callback](SokuData::KeyboardData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushinteger(L, data.key);
        lua_pushinteger(L, data.lParam);
        if (lua_pcall(L, 2, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeBattleEvent(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::BattleEvent, [L, callback](SokuData::BattleEventData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushinteger(L, (int)data.event);
        if (lua_pcall(L, 1, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeGameEvent(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::GameEvent, [L, callback](SokuData::GameEventData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushinteger(L, (int)data.event);
        lua_pushinteger(L, (int)data.ptr);
        if (lua_pcall(L, 2, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeStageSelect(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::StageSelect, [L, callback](SokuData::StageData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushinteger(L, (int)data.stage);
        if (lua_pcall(L, 1, 1, 0)) {
            Logger::Error(lua_tostring(L, -1));
        } else {
            if (lua_isnumber(L, -1)) data.stage = (Stage)lua_tointeger(L, -1);
        } lua_pop(L, 1);
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeFileLoader(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::FileLoader, [L, callback](SokuData::FileLoaderData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushstring(L, data.fileName);
        if (lua_pcall(L, 1, 2, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
            return;
        } switch (lua_type(L, 1)) {
            case LUA_TSTRING: {
                const char* format = lua_tostring(L, 2);
                if (format && strcmp(format, "png") == 0)
                    data.inputFormat = DataFormat::PNG;
                else data.inputFormat = DataFormat::RAW;
                const char* stream = lua_tolstring(L, 1, (size_t*)&data.size);
                if (!stream || !data.size) break;
                data.data = new char[data.size];
                memcpy(data.data, stream, data.size);
            } break;
            case LUA_TUSERDATA: {
                std::stringstream* buffer = new std::stringstream();
                ShadyCore::Resource* resource = Stack<ShadyCore::Resource*>::get(L, 1);
                ShadyCore::writeResource(resource, *buffer, true);
                data.inputFormat = DataFormat::RAW;
                setup_stream_reader(data, buffer, buffer->tellp());
            } break;
        }
        lua_pop(L, 2);
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeInput(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
    subscribeMap[callback] = Soku::SubscribeEvent(SokuEvent::Input, [L, callback](SokuData::InputData& data) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        // TODO
        if (lua_pcall(L, 0, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_pushnumber(L, callback);
    return 1;
}

static int soku_SubscribeEvent(lua_State* L) {
    SokuEvent evt = (SokuEvent)luaL_checkinteger(L, 1);
    lua_remove(L, 1);

    switch(evt) {
        case SokuEvent::Render: return soku_SubscribeRender(L);
        case SokuEvent::WindowProc: return soku_SubscribeWindowProc(L);
        case SokuEvent::Keyboard: return soku_SubscribeKeyboard(L);
        case SokuEvent::BattleEvent: return soku_SubscribeBattleEvent(L);
        case SokuEvent::GameEvent: return soku_SubscribeGameEvent(L);
        case SokuEvent::StageSelect: return soku_SubscribeStageSelect(L);
        case SokuEvent::FileLoader: return soku_SubscribeFileLoader(L);
        case SokuEvent::Input: return soku_SubscribeInput(L);
        default: return luaL_error(L, "Event %d not found", evt);
    }
}

static void soku_UnsubscribeEvent(int id, lua_State* L) {
    Soku::UnsubscribeEvent(subscribeMap.at(id));
    luaL_unref(L, LUA_REGISTRYINDEX, id);
    subscribeMap.erase(id);
}

void ShadyLua::LualibSoku(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("soku")
            .beginNamespace("Event")
                .addVariable("Render", enumMap<(int)SokuEvent::Render>(), false)
                .addVariable("WindowProc", enumMap<(int)SokuEvent::WindowProc>(), false)
                .addVariable("Keyboard", enumMap<(int)SokuEvent::Keyboard>(), false)
                .addVariable("BattleEvent", enumMap<(int)SokuEvent::BattleEvent>(), false)
                .addVariable("GameEvent", enumMap<(int)SokuEvent::GameEvent>(), false)
                .addVariable("StageSelect", enumMap<(int)SokuEvent::StageSelect>(), false)
                .addVariable("FileLoader", enumMap<(int)SokuEvent::FileLoader>(), false)
                .addVariable("Input", enumMap<(int)SokuEvent::Input>(), false)
            .endNamespace()
            .beginNamespace("BattleEvent")
                .addVariable("GameStart", enumMap<(int)BattleEvent::GameStart>(), false)
                .addVariable("RoundPreStart", enumMap<(int)BattleEvent::RoundPreStart>(), false)
                .addVariable("RoundStart", enumMap<(int)BattleEvent::RoundStart>(), false)
                .addVariable("RoundEnd", enumMap<(int)BattleEvent::RoundEnd>(), false)
                .addVariable("GameEnd", enumMap<(int)BattleEvent::GameEnd>(), false)
                .addVariable("EndScreen", enumMap<(int)BattleEvent::EndScreen>(), false)
                .addVariable("ResultScreen", enumMap<(int)BattleEvent::ResultScreen>(), false)
            .endNamespace()
            .beginNamespace("Character")
                .addVariable("Reimu", enumMap<(int)Character::Reimu>(), false)
                .addVariable("Marisa", enumMap<(int)Character::Marisa>(), false)
                .addVariable("Sakuya", enumMap<(int)Character::Sakuya>(), false)
                .addVariable("Alice", enumMap<(int)Character::Alice>(), false)
                .addVariable("Patchouli", enumMap<(int)Character::Patchouli>(), false)
                .addVariable("Youmu", enumMap<(int)Character::Youmu>(), false)
                .addVariable("Remilia", enumMap<(int)Character::Remilia>(), false)
                .addVariable("Yuyuko", enumMap<(int)Character::Yuyuko>(), false)
                .addVariable("Yukari", enumMap<(int)Character::Yukari>(), false)
                .addVariable("Suika", enumMap<(int)Character::Suika>(), false)
                .addVariable("Reisen", enumMap<(int)Character::Reisen>(), false)
                .addVariable("Aya", enumMap<(int)Character::Aya>(), false)
                .addVariable("Komachi", enumMap<(int)Character::Komachi>(), false)
                .addVariable("Iku", enumMap<(int)Character::Iku>(), false)
                .addVariable("Tenshi", enumMap<(int)Character::Tenshi>(), false)
                .addVariable("Sanae", enumMap<(int)Character::Sanae>(), false)
                .addVariable("Cirno", enumMap<(int)Character::Cirno>(), false)
                .addVariable("Meiling", enumMap<(int)Character::Meiling>(), false)
                .addVariable("Utsuho", enumMap<(int)Character::Utsuho>(), false)
                .addVariable("Suwako", enumMap<(int)Character::Suwako>(), false)
                .addVariable("Random", enumMap<(int)Character::Random>(), false)
                .addVariable("Namazu", enumMap<(int)Character::Namazu>(), false)
            .endNamespace()
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
            .beginNamespace("Scene")
                .addVariable("Logo", enumMap<(int)SCENE::LOGO>(), false)
                .addVariable("Opening", enumMap<(int)SCENE::OPENING>(), false)
                .addVariable("Title", enumMap<(int)SCENE::TITLE>(), false)
                .addVariable("Select", enumMap<(int)SCENE::SELECT>(), false)
                .addVariable("Battle", enumMap<(int)SCENE::BATTLE>(), false)
                .addVariable("SelectSV", enumMap<(int)SCENE::SELECTSV>(), false)
                .addVariable("SelectCL", enumMap<(int)SCENE::SELECTCL>(), false)
                .addVariable("LoadingSV", enumMap<(int)SCENE::LOADINGSV>(), false)
                .addVariable("LoadingCL", enumMap<(int)SCENE::LOADINGCL>(), false)
                .addVariable("LoadingWatch", enumMap<(int)SCENE::LOADINGWATCH>(), false)
                .addVariable("BattleSV", enumMap<(int)SCENE::BATTLESV>(), false)
                .addVariable("BattleCL", enumMap<(int)SCENE::BATTLECL>(), false)
                .addVariable("BattleWatch", enumMap<(int)SCENE::BATTLEWATCH>(), false)
                .addVariable("SelectScenario", enumMap<(int)SCENE::SELECTSENARIO>(), false)
                .addVariable("Ending", enumMap<(int)SCENE::ENDING>(), false)
                .addVariable("Submenu", enumMap<(int)SCENE::SUBMENU>(), false)
            .endNamespace()
            .beginNamespace("Stage")
                .addVariable("RepairedHakureiShrine", enumMap<(int)Stage::RepairedHakureiShrine>(), false)
                .addVariable("ForestOfMagic", enumMap<(int)Stage::ForestOfMagic>(), false)
                .addVariable("CreekOfGenbu", enumMap<(int)Stage::CreekOfGenbu>(), false)
                .addVariable("YoukaiMountain", enumMap<(int)Stage::YoukaiMountain>(), false)
                .addVariable("MysteriousSeaOfClouds", enumMap<(int)Stage::MysteriousSeaOfClouds>(), false)
                .addVariable("BhavaAgra", enumMap<(int)Stage::BhavaAgra>(), false)
                .addVariable("HakureiShrine", enumMap<(int)Stage::HakureiShrine>(), false)
                .addVariable("KirisameMagicShop", enumMap<(int)Stage::KirisameMagicShop>(), false)
                .addVariable("SDMClockTower", enumMap<(int)Stage::SDMClockTower>(), false)
                .addVariable("ForestOfDolls", enumMap<(int)Stage::ForestOfDolls>(), false)
                .addVariable("SDMLibrary", enumMap<(int)Stage::SDMLibrary>(), false)
                .addVariable("Netherworld", enumMap<(int)Stage::Netherworld>(), false)
                .addVariable("SDMFoyer", enumMap<(int)Stage::SDMFoyer>(), false)
                .addVariable("HakugyokurouSnowyGarden", enumMap<(int)Stage::HakugyokurouSnowyGarden>(), false)
                .addVariable("BambooForestOfTheLost", enumMap<(int)Stage::BambooForestOfTheLost>(), false)
                .addVariable("ShoreOfMistyLake", enumMap<(int)Stage::ShoreOfMistyLake>(), false)
                .addVariable("MoriyaShrine", enumMap<(int)Stage::MoriyaShrine>(), false)
                .addVariable("MouthOfGeyser", enumMap<(int)Stage::MouthOfGeyser>(), false)
                .addVariable("CatwalkInGeyser", enumMap<(int)Stage::CatwalkInGeyser>(), false)
                .addVariable("FusionReactorCore", enumMap<(int)Stage::FusionReactorCore>(), false)
            .endNamespace()
            .beginClass<Soku::CPlayer>("Player")
                .addProperty("Character", soku_Player_Character)
                .addFunction("PlaySE", soku_Player_PlaySE)
            .endClass()
            .addVariable("P1", &Soku::P1)
            .addVariable("P2", &Soku::P2)
            .addFunction("Module", soku_Module)
            .addFunction("PlaySE", Soku::PlaySE)
            .addFunction("ReloadSE", soku_ReloadSE)
            .addCFunction("SubscribeEvent", soku_SubscribeEvent)
            .addCFunction("SubscribeRender", soku_SubscribeRender)
            .addCFunction("SubscribeWindowProc", soku_SubscribeWindowProc)
            .addCFunction("SubscribeKeyboard", soku_SubscribeKeyboard)
            .addCFunction("SubscribeBattleEvent", soku_SubscribeBattleEvent)
            .addCFunction("SubscribeGameEvent", soku_SubscribeGameEvent)
            .addCFunction("SubscribeStageSelect", soku_SubscribeStageSelect)
            .addCFunction("SubscribeFileLoader", soku_SubscribeFileLoader)
            .addCFunction("SubscribeInput", soku_SubscribeInput)
            .addFunction("UnsubscribeEvent", soku_UnsubscribeEvent)
        .endNamespace()
    ;
}