#include "../Core/package.hpp"
#include "modpackage.hpp"
#include "main.hpp"
#include "../Lua/script.hpp"
#include "../Lua/lualibs.hpp"

namespace {
    struct _lua_file {
        ShadyCore::BasePackageEntry* entry;
        std::istream* input;
    };
}

void* _lua_open(void* userdata, const char* filename) {
    ShadyCore::Package* package = reinterpret_cast<ShadyCore::Package*>(userdata);
    auto iter = package->find(filename);
    if (iter == package->end()) return 0;
    return new _lua_file{iter->second, &iter->second->open()};
}

size_t _lua_read(void* userdata, void* file, char* buffer, size_t size) {
    if (!file) return 0;
    _lua_file* luaFile = reinterpret_cast<_lua_file*>(file);
    luaFile->input->read(buffer, size);
    size = luaFile->input->gcount();
    if (size == 0) {
        luaFile->entry->close();
        delete luaFile;
    } return size;
}

void EnablePackage(ModPackage* p) {
    if (!std::filesystem::is_directory(p->path) && !std::filesystem::is_regular_file(p->path)) return;

    p->package = ModPackage::basePackage->merge(p->path);

    bool initFound = false;
    auto iter = p->package->find("init.lua");
    if (iter != p->package->end()) initFound = true;
    else {
        iter = ModPackage::basePackage->find("init.lua");
        if (iter != ModPackage::basePackage->end()) initFound = true;
    }

    if (initFound && iter.entry().getParent() == p->package) {
        ShadyLua::LuaScript* script = new ShadyLua::LuaScript(ModPackage::basePackage.get(), _lua_open, _lua_read);
        ShadyLua::LualibBase(script->L, ModPackage::basePath);
        ShadyLua::LualibMemory(script->L);
        ShadyLua::LualibResource(script->L);
        ShadyLua::LualibSoku(script->L);
        // TODO this doesn't work right
        if (script->load("init.lua") != LUA_OK || !script->run()) {
            delete script;
            p->script = 0;
        } else {
            p->script = script;
        }
    }
}

void DisablePackage(ModPackage* p) {
    if (p->script) delete (ShadyLua::LuaScript*)p->script;
    if (p->package) ModPackage::basePackage->erase(p->package);
    p->script = p->package = 0;
}