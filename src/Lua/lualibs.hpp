#pragma once
#include <lua.hpp>
#include <filesystem>

namespace ShadyLua {
    void LualibBase(lua_State* L, std::filesystem::path& libPath);
    void LualibMemory(lua_State* L);
    void LualibResource(lua_State* L);
    void LualibSoku(lua_State* L);
    //void LualibImGui(lua_State* L);

    // TODO arguments on Emitters
    void EmitSokuEventRender();
    void EmitSokuEventBattleEvent();
    void EmitSokuEventGameEvent();
    void EmitSokuEventStageSelect();
    void EmitSokuEventFileLoader(const char* filename, std::istream **input, int*size);
}