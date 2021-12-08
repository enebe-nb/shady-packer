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

    struct _lua_loader {
        ShadyCore::Package* base;
        ShadyCore::Package* owner;
    };
}

static ShadyCore::BasePackageEntry* _lua_find(void* userdata, const char* filename) {
    _lua_loader* loader = reinterpret_cast<_lua_loader*>(userdata);
    auto iter = loader->owner->find(filename);
    if (iter == loader->owner->end()) {
        iter = loader->base->find(filename);
        if (iter == loader->base->end()) return 0;
    }

    if (iter.entry().getParent() != loader->owner) return 0;
    else return &iter.entry();
}

static void* _lua_open(void* userdata, const char* filename) {
    ShadyCore::BasePackageEntry* entry = _lua_find(userdata, filename);
    if (entry) return new _lua_file{entry, &entry->open()};
    else return 0;
}

static size_t _lua_read(void* userdata, void* file, char* buffer, size_t size) {
    if (!file) return 0;
    _lua_file* luaFile = reinterpret_cast<_lua_file*>(file);
    luaFile->input->read(buffer, size);
    size = luaFile->input->gcount();
    if (size == 0) {
        luaFile->entry->close();
        delete luaFile;
    } return size;
}

static void _lua_destroy(void* userdata) {
    delete reinterpret_cast<_lua_loader*>(userdata);
}

void EnablePackage(ModPackage* p) {
    if (!std::filesystem::is_directory(p->path) && !std::filesystem::is_regular_file(p->path)) return;
    p->package = ModPackage::basePackage->merge(p->path);

    std::shared_lock l0(*ModPackage::basePackage, std::defer_lock);
    std::shared_lock l1(*p->package, std::defer_lock);
    std::scoped_lock lock(l0, l1);
    auto loader = new _lua_loader{ModPackage::basePackage.get(), p->package};

    if (_lua_find(loader, "init.lua")) {
        ShadyLua::LuaScript* script = new ShadyLua::LuaScript(loader, _lua_open, _lua_read, _lua_destroy);
        ShadyLua::LualibBase(script->L, ModPackage::basePath);
        ShadyLua::LualibMemory(script->L);
        ShadyLua::LualibResource(script->L);
        ShadyLua::LualibSoku(script->L);
        if (script->load("init.lua") != LUA_OK || !script->run()) {
            delete script;
            p->script = 0;
        } else {
            p->script = script;
        }
    } else delete loader;
}

void DisablePackage(ModPackage* p) {
    if (p->script) delete (ShadyLua::LuaScript*)p->script;
    if (p->package) ModPackage::basePackage->erase(p->package);
    p->script = p->package = 0;
}