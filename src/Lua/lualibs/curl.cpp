#include "../lualibs.hpp"

extern "C" __declspec(dllexport) int luaopen_lcurl(lua_State *L);
extern "C" __declspec(dllexport) int luaopen_lcurl_safe(lua_State *L);

void ShadyLua::LualibCurl(lua_State*L) {
    luaopen_lcurl(L);
    luaopen_lcurl_safe(L);
}