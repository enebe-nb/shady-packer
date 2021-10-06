#include "../Core/package.hpp"
#include "modpackage.hpp"
#include "main.hpp"
#include "../Lua/script.hpp"
#include "../Lua/lualibs.hpp"

extern ShadyCore::Package package;
namespace {
    struct _lua_file {
        ShadyCore::BasePackageEntry* entry;
        std::istream* input;
    };
}

static void applyGameFilters(int id = -1) {
    ShadyCore::PackageFilter::apply(package, 
        (ShadyCore::PackageFilter::Filter)(
            ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION |
            ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE |
            ShadyCore::PackageFilter::FILTER_TO_LOWERCASE
        ), id);
}



void* _lua_open(void* userdata, const char* filename) {
    ShadyCore::Package* package = reinterpret_cast<ShadyCore::Package*>(userdata);
    auto iter = package->findFile(filename);
    if (iter == package->end()) return 0;
    return new _lua_file{&*iter, &iter->open()};
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
	std::filesystem::path path(p->name); path += p->ext;
	if (path.is_relative()) path = ModPackage::basePath / path;

    if (std::filesystem::is_directory(path)
        || std::filesystem::is_regular_file(path)) {
        
        //std::string pathu8 = ws2s(path);
        //p->packageId = package.appendPackage(pathu8.c_str());
        p->packageId = package.appendPackage(std::filesystem::relative(path).string().c_str());
        applyGameFilters(p->packageId);
    }

    auto iter = package.findFile("init.lua"); // TODO verify validity
    if (iter != package.end() && iter->getId() == p->packageId) {
        ShadyLua::LuaScript* script = new ShadyLua::LuaScript(&package, _lua_open, _lua_read);
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
    }
}

void DisablePackage(ModPackage* p) {
    if (p->script) delete (ShadyLua::LuaScript*)p->script;
    if (p->packageId >= 0) package.detach(p->packageId);
}