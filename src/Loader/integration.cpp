#include "../Core/package.hpp"
#include "modpackage.hpp"
#include "main.hpp"
#include "../Lua/script.hpp"
#include "../Lua/lualibs.hpp"
#include "../Lua/lualibs/soku.hpp"
#include <LuaBridge/LuaBridge.h>
#include "../Lua/logger.hpp"
#include "../Core/util/tempfiles.hpp"
#include <fstream>

namespace {
    struct _lua_file : ShadyLua::LuaScript::File {
        ShadyCore::BasePackageEntry* entry;
        inline _lua_file(ShadyCore::BasePackageEntry* entry) : entry(entry), File(entry->open()) {}
    };

    struct _lua_loader {
        ShadyCore::Package* base;
        ShadyCore::Package* owner;
    };

	struct charbuf : std::streambuf {
		inline charbuf(const char* begin, const char* end) {
			this->setg((char*)begin, (char*)begin, (char*)end);
		}
	};
}

static bool loader_addAlias(const char* alias, const char* target) {
	auto iter = ModPackage::basePackage->find(target);
	if (iter == ModPackage::basePackage->end()) return false;
	return ModPackage::basePackage->alias(alias, iter.entry()) != ModPackage::basePackage->end();
}

static int loader_addData(lua_State* L) {
	const char* alias = luaL_checkstring(L, 1);
	size_t dataSize; const char* data = luaL_checklstring(L, 2, &dataSize);
	charbuf buffer(data, data+dataSize);
	std::istream input(&buffer);

	lua_pushboolean(L, ModPackage::basePackage->insert(alias, input) != ModPackage::basePackage->end());
	return 1;
}

static bool loader_removeFile(const char* alias, lua_State* L) {
	auto iter = ModPackage::basePackage->find(alias);
	if (iter == ModPackage::basePackage->end()) return false;
	auto loader = reinterpret_cast<_lua_loader*>(ShadyLua::ScriptMap[L]->userdata);
	if (iter.entry().getParent() != loader->base
		&& iter.entry().getParent() != loader->owner) {
		Logger::Error("Can't remove files from other mods.");
		return false;
	}

	return ModPackage::basePackage->erase(alias);
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

static ShadyLua::LuaScript::File* _lua_open(void* userdata, const char* filename) {
    _lua_loader* loader = reinterpret_cast<_lua_loader*>(userdata);
    std::shared_lock l0(*loader->base, std::defer_lock);
    std::shared_lock l1(*loader->owner, std::defer_lock);
    std::scoped_lock lock(l0, l1);

    ShadyCore::BasePackageEntry* entry = _lua_find(loader->base, loader->owner, filename);
    if (entry) return new _lua_file(entry);
    else return 0;
}

static void _lua_close(void* userdata, ShadyLua::LuaScript::File* file) {
    _lua_file* _file = reinterpret_cast<_lua_file*>(file);
    _file->entry->close();
    delete file;
}

static void _lua_destroy(void* userdata) {
    delete reinterpret_cast<_lua_loader*>(userdata);
}

static void LualibLoader(lua_State* L) {
	luabridge::getGlobalNamespace(L)
		.beginNamespace("loader")
			.addFunction("addAlias", loader_addAlias)
			.addFunction("addData", loader_addData)
			.addFunction("removeFile", loader_removeFile)
		.endNamespace();
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
        ShadyLua::LuaScript* script = new ShadyLua::LuaScript(loader, _lua_open, _lua_close, _lua_destroy);
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