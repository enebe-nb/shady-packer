#include "../lualibs.hpp"
#include "../script.hpp"
#include "../../Core/resource.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedPtr.h>
#include <unordered_map>
#include <sstream>

using namespace luabridge;
extern std::unordered_map<lua_State*, ShadyLua::LuaScript*> scriptMap;

namespace {
    struct StringInputBuf : public std::streambuf {
        StringInputBuf(std::string& s) {setg((char*)s.data(), (char*)s.data(), (char*)(s.data() + s.size()));}
    };
}

/** Creates a resource from a file in script space */
static RefCountedPtr<ShadyCore::Resource> resource_createfromfile(const char* filename, lua_State* L) {
    ShadyLua::LuaScript* script = scriptMap.at(L);

    std::stringstream data;
    char buffer[4096]; size_t size;
    void* file = script->open(filename);
    while (size = script->read(file, buffer, 4096)) {
        data.write(buffer, size);
    }

    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename, data);
    return ShadyCore::readResource(type, data);
}

void ShadyLua::LualibResource(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("resource")
            .addFunction("createfromfile", resource_createfromfile)
            .beginClass<ShadyCore::Resource>("Resource")
            .endClass()
        .endNamespace()
    ;
}