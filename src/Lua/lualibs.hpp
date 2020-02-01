#pragma once
#include <lua.hpp>

namespace ShadyLua {
    void LualibBase(lua_State* L);
    void LualibMemory(lua_State* L);
    void LualibResource(lua_State* L);
    void LualibSoku(lua_State* L);
    void LualibImGui(lua_State* L);

    inline void LualibAll(lua_State* L) {
        LualibBase(L);
        LualibMemory(L);
        LualibResource(L);
        LualibSoku(L);
        LualibImGui(L);
    }
}