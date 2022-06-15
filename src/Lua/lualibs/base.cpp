#include "../lualibs.hpp"
#include "../logger.hpp"
#include "../script.hpp"
#include <LuaBridge/LuaBridge.h>

static int _print(lua_State* L) {
    std::string line; size_t size;
    int nargs = lua_gettop(L);
    for (int i = 1; i <= nargs; ++i) {
        if (i != 1) line += " ";
        const char* str = luaL_tolstring(L, i, &size);
        line.append(str, size);
    }

    Logger::Debug(line);
    return 0;
}

static int _loadfile(lua_State* L) {
    ShadyLua::LuaScript* script = ShadyLua::ScriptMap.at(L);
    const char *filename = luaL_checkstring(L, 1);
    const char *mode = luaL_optstring(L, 2, NULL);
    int env = (!lua_isnone(L, 3) ? 3 : 0);

    if (script->load(filename, mode)) {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    } else if (env) {
        lua_pushvalue(L, env);
        if (!lua_setupvalue(L, -2, 1)) lua_pop(L, 1);
    } return 1;
}

static int _readfile(lua_State* L) {
    ShadyLua::LuaScript* script = ShadyLua::ScriptMap.at(L);
    const char *filename = luaL_checkstring(L, 1);
    auto file = script->openFile(filename);

    luaL_Buffer buffer; luaL_buffinit(L, &buffer);
    do {
        char* addr = luaL_prepbuffer(&buffer);
        file->input.read(addr, LUAL_BUFFERSIZE);
        luaL_addsize(&buffer, file->input.gcount());
    } while (file->input.gcount());

    script->closeFile(file);
    luaL_pushresult(&buffer);
    return 1;
}

static int _dofile(lua_State* L) {
    ShadyLua::LuaScript* script = ShadyLua::ScriptMap.at(L);
    const char *filename = luaL_checkstring(L, 1);
    lua_settop(L, 1);
    if (script->load(filename)) return lua_error(L);
    lua_call(L, 0, LUA_MULTRET);
    return lua_gettop(L) - 1;
}

void ShadyLua::LualibBase(lua_State* L) {
    luaL_openlibs(L);
    lua_register(L, "print", _print);
    lua_register(L, "loadfile", _loadfile);
    lua_register(L, "readfile", _readfile);
    lua_register(L, "dofile", _dofile);
}