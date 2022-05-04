#include "../Core/package.hpp"
#include "modpackage.hpp"
#include "main.hpp"
#include "../Lua/script.hpp"
#include "../Lua/lualibs.hpp"
#include <LuaBridge/LuaBridge.h>
#include "../Lua/logger.hpp"

namespace {
    struct _lua_file {
        ShadyCore::BasePackageEntry* entry;
        std::istream* input;
    };

    struct _lua_loader {
        ShadyCore::Package* base;
        ShadyCore::Package* owner;
    };

    struct charbuf : std::streambuf {
        charbuf(const char* begin, const char* end) {
            this->setg((char*)begin, (char*)begin, (char*)end);
        }
    };
}

static bool loader_alias(const char* alias, const char* target) {
    auto iter = ModPackage::basePackage->find(target);
    if (iter == ModPackage::basePackage->end()) return false;
    ModPackage::basePackage->alias(alias, iter.entry());
    return true;
}

static bool loader_data(const char* alias, const std::string data) {
    charbuf buffer(data.data(), data.data() + data.size());
    std::istream stream(&buffer);
    ModPackage::basePackage->insert(alias, stream);
    return true;
}

static bool loader_resource(const char* alias, ShadyCore::Resource* data) {
    auto ft = ShadyCore::FileType::get(alias);
    if (ft.format == ShadyCore::FileType::FORMAT_UNKNOWN) throw std::exception("File format is not supported");
    std::stringstream stream;
    ShadyCore::getResourceWriter(ft)(data, stream);
    ModPackage::basePackage->insert(alias, stream);
    return true;
}

static ShadyCore::BasePackageEntry* _lua_find(ShadyCore::Package* base, ShadyCore::Package* owner, const char* filename) {
    auto iter = owner->find(filename);
    if (iter == owner->end()) {
        iter = base->find(filename);
        if (iter == base->end()) return 0;
    }

    if (iter.entry().getParent() != owner) return 0;
    else return &iter.entry();
}

static void* _lua_open(void* userdata, const char* filename) {
    // TODO fix and test mutexes here
    _lua_loader* loader = reinterpret_cast<_lua_loader*>(userdata);
    ShadyCore::BasePackageEntry* entry = _lua_find(loader->base, loader->owner, filename);
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

static void LualibLoader(lua_State* L) {
    luabridge::getGlobalNamespace(L)
        .beginNamespace("loader")
            .addFunction("insertAlias", loader_alias)
            .addFunction("insertData", loader_data)
            .addFunction("insertResource", loader_resource)
        .endNamespace()
    ;
}

void EnablePackage(ModPackage* p) {
    if (!std::filesystem::is_directory(p->path) && !std::filesystem::is_regular_file(p->path)) return;
    p->package = ModPackage::basePackage->merge(p->path);
    if (!iniEnableLua) return;

    { std::shared_lock l0(*ModPackage::basePackage, std::defer_lock);
    std::shared_lock l1(*p->package, std::defer_lock);
    std::scoped_lock lock(l0, l1);

    if (_lua_find(ModPackage::basePackage.get(), p->package, "init.lua")) {
        auto loader = new _lua_loader{ModPackage::basePackage.get(), p->package};
        ShadyLua::LuaScript* script = new ShadyLua::LuaScript(loader, _lua_open, _lua_read, _lua_destroy);
        ShadyLua::LualibBase(script->L, ModPackage::basePath);
        ShadyLua::LualibMemory(script->L);
        ShadyLua::LualibResource(script->L);
        ShadyLua::LualibSoku(script->L);
        LualibLoader(script->L);
        p->script = script;
    } }

    if (p->script) {
        if (((ShadyLua::LuaScript*)p->script)->load("init.lua") != LUA_OK || !((ShadyLua::LuaScript*)p->script)->run()) {
            delete ((ShadyLua::LuaScript*)p->script);
            p->script = 0;
        }
    }
}

void DisablePackage(ModPackage* p) {
    if (p->script) delete (ShadyLua::LuaScript*)p->script;
    if (p->package) ModPackage::basePackage->erase(p->package);
    p->script = p->package = 0;
}