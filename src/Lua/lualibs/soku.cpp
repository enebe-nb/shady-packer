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
#include <unordered_set>
#include <SokuLib.hpp>

using namespace luabridge;

namespace {
    constexpr int EVENT_RENDER = 0;
    constexpr int EVENT_LOADER = 1;

    struct eventHash { size_t operator()(const std::pair<int, ShadyLua::LuaScript*> &x) const { return x.first ^ (int)x.second; } };
    std::unordered_set<std::pair<int, ShadyLua::LuaScript*>, eventHash> eventMap[2];
    std::shared_mutex eventMapLock;
    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}

    struct charbuf : std::streambuf {
        charbuf(const char* begin, const char* end) {
            this->setg((char*)begin, (char*)begin, (char*)end);
        }
    };
}

void ShadyLua::RemoveEvents(LuaScript* script) {
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

static void soku_ReloadSE(int id) {
    char buffer[] = "data/se/000.wav";
    sprintf(buffer, "data/se/%03d.wav", id);
    // TODO fix call
    __asm mov ecx, 0x0089f9f8;
    reinterpret_cast<void(__stdcall*)(int*, char*)>(0x00401af0)((int*)(0x00899D60 + id*4), buffer);
}

void ShadyLua::EmitSokuEventRender() {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[EVENT_RENDER];
    for(auto iter = listeners.begin(); iter != listeners.end(); ++iter) {
        std::lock_guard scriptGuard(iter->second->mutex);

        lua_State* L = iter->second->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->first);
        if (lua_pcall(L, 0, 0, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void ShadyLua::EmitSokuEventFileLoader(const char* filename, std::istream** input, int* size) {
    std::shared_lock guard(eventMapLock);
    auto& listeners = eventMap[EVENT_LOADER];
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
                // TODO fix filetype
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
            } break;
            /*
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
            */
        }
        lua_pop(L, 2);
    }
}

template<int eventType>
static int soku_SubscribeEvent(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) return luaL_error(L, "Must pass a callback");
    int callback = luaL_ref(L, LUA_REGISTRYINDEX);
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
            .beginClass<SokuLib::PlayerInfo>("Player")
                .addProperty("character", &SokuLib::PlayerInfo::character, false)
                .addProperty("isRight", &SokuLib::PlayerInfo::isRight, false)
                .addProperty("palette", &SokuLib::PlayerInfo::palette, false)
                .addProperty("deck", &SokuLib::PlayerInfo::deck, false)
                .addFunction("PlaySE", soku_Player_PlaySE)
            .endClass()
            .addVariable("P1", &SokuLib::leftPlayerInfo)
            .addVariable("P2", &SokuLib::rightPlayerInfo)
            .addFunction("PlaySE", soku_PlaySE)
            .addFunction("ReloadSE", soku_ReloadSE)
            .addCFunction("SubscribeRender", soku_SubscribeEvent<EVENT_RENDER>)
            .addCFunction("SubscribeFileLoader", soku_SubscribeEvent<EVENT_LOADER>)
            .addFunction("UnsubscribeEvent", soku_UnsubscribeEvent)
        .endNamespace()
    ;
}