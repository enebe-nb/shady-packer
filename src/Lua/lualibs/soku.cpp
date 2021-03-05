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
const int soku_EventGameEvent = 4;
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

void ShadyLua::EmitSokuEventBattleEvent() {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventBattleEvent];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
    //     lua_pushinteger(L, (int)data.event);
        if (lua_pcall(L, 0, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void ShadyLua::EmitSokuEventGameEvent() {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventGameEvent];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
    //     lua_pushinteger(L, (int)data.event);
    //     lua_pushinteger(L, (int)data.ptr);
        if (lua_pcall(L, 0, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void ShadyLua::EmitSokuEventStageSelect(int stageId) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[soku_EventStageSelect];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        lua_pushinteger(L, stageId);
        if (lua_pcall(L, 1, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
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
                if (format) {
                    auto& type = ShadyCore::FileType::getByName(format);
                    if (!type.isEncrypted && type != ShadyCore::FileType::TYPE_UNKNOWN) {
                        charbuf decBuf(data, data + dataSize);
                        std::istream decStream(&decBuf);
                        std::stringstream* encStream = 
                            new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
                        ShadyCore::convertResource(type, decStream, *encStream);
                        *size = encStream->tellp();
                        *input = encStream;
                        lua_pop(L, 2);
                        return;
                    }
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
                ShadyCore::writeResource(resource.get(), *buffer, true);
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

void ShadyLua::LualibSoku(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("soku")
            .beginNamespace("Event")
                .addVariable("Render", (int*)&soku_EventRender, false)
                //.addVariable("WindowProc", enumMap<(int)SokuEvent::WindowProc>(), false)
                //.addVariable("Keyboard", enumMap<(int)SokuEvent::Keyboard>(), false)
                .addVariable("BattleEvent", (int*)&soku_EventBattleEvent, false)
                .addVariable("GameEvent", (int*)&soku_EventGameEvent, false)
                .addVariable("StageSelect", (int*)&soku_EventStageSelect, false)
                .addVariable("FileLoader", (int*)&soku_EventFileLoader, false)
                //.addVariable("Input", enumMap<(int)SokuEvent::Input>(), false)
            .endNamespace()
            /*
            .beginNamespace("BattleEvent")
                .addVariable("GameStart", enumMap<(int)BattleEvent::GameStart>(), false)
                .addVariable("RoundPreStart", enumMap<(int)BattleEvent::RoundPreStart>(), false)
                .addVariable("RoundStart", enumMap<(int)BattleEvent::RoundStart>(), false)
                .addVariable("RoundEnd", enumMap<(int)BattleEvent::RoundEnd>(), false)
                .addVariable("GameEnd", enumMap<(int)BattleEvent::GameEnd>(), false)
                .addVariable("EndScreen", enumMap<(int)BattleEvent::EndScreen>(), false)
                .addVariable("ResultScreen", enumMap<(int)BattleEvent::ResultScreen>(), false)
            .endNamespace()
            */
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
            */
            //.beginClass<Soku::CPlayer>("Player")
                //.addProperty("Character", soku_Player_Character)
                //.addFunction("PlaySE", soku_Player_PlaySE)
            //.endClass()
            //.addVariable("P1", &Soku::P1)
            //.addVariable("P2", &Soku::P2)
            //.addFunction("PlaySE", Soku::PlaySE)
            //.addFunction("ReloadSE", soku_ReloadSE)
            .addCFunction("SubscribeEvent", soku_SubscribeEvent)
            .addCFunction("SubscribeRender", soku_SubscribeEventTyped<soku_EventRender>)
            //.addCFunction("SubscribeWindowProc", soku_SubscribeWindowProc)
            //.addCFunction("SubscribeKeyboard", soku_SubscribeKeyboard)
            .addCFunction("SubscribeBattleEvent", soku_SubscribeEventTyped<soku_EventBattleEvent>)
            .addCFunction("SubscribeGameEvent", soku_SubscribeEventTyped<soku_EventGameEvent>)
            .addCFunction("SubscribeStageSelect", soku_SubscribeEventTyped<soku_EventStageSelect>)
            .addCFunction("SubscribeFileLoader", soku_SubscribeEventTyped<soku_EventFileLoader>)
            //.addCFunction("SubscribeInput", soku_SubscribeInput)
            .addFunction("UnsubscribeEvent", soku_UnsubscribeEvent)
        .endNamespace()
    ;
}