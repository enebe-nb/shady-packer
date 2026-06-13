#pragma once
#include <lua.hpp>
#include <filesystem>
#include <LuaBridge/LuaBridge.h>
#include "lualibs/soku.hpp"
#include "../Core/package.hpp"

namespace ShadyLua {
    void LualibBase(lua_State* L);
    void LualibMemory(lua_State* L);
    void LualibResource(lua_State* L);
    void LualibSoku(lua_State* L);
    void LualibLoader(lua_State* L, ShadyCore::PackageEx* package);
    void LualibGui(lua_State* L);
    void LualibBattle(lua_State* L);


    namespace {
        using t_copyf = void(lua_State*, int, lua_State*);
        template<typename T> DECLSPEC_NOINLINE
        static void userdata_copier(lua_State* Ls, int index, lua_State* Lt) {
            luabridge::Stack<T>::push(Lt, luabridge::Stack<T>::get(Ls, index));
        }
    }
    void _insert_map_helper(lua_State*, const void*, t_copyf*);
    template<typename T, bool is_cdata = true>
    void RegisterIPCUserdata(lua_State* L) {
        auto kreg = luabridge::detail::getClassRegistryKey<T>();
        lua_rawgetp(L, LUA_REGISTRYINDEX, kreg);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::runtime_error("IPC: target userdata type not registered in LuaBridge");
        }
        auto pmt = lua_topointer(L, -1);
        t_copyf* pf = nullptr;
        if constexpr (is_cdata) pf = &userdata_copier<T*>; else pf = &userdata_copier<T>;
		_insert_map_helper(L, pmt, pf);
        lua_pop(L, 1);
    }
}