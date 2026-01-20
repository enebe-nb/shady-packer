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

		pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) {
			if (dir == std::ios_base::cur) gbump(off);
			else if (dir == std::ios_base::end)
				setg(eback(), egptr() + off, egptr());
			else if (dir == std::ios_base::beg)
				setg(eback(), eback() + off, egptr());
			return gptr() - eback();
		}

		pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) {
			return seekoff(pos, std::ios::beg);
		}
	};

	struct ExposedMods {
		std::vector<const char*> list;
		inline ExposedMods() { list.push_back(NULL); }
		inline ~ExposedMods() {
			for (auto mod : list) if (mod) delete[] mod;
		}

		void add(ModPackage* package) {
			for (auto mod : list) if (mod && package->name == mod) return;
			size_t size = package->name.size();
			char* name = new char[size + 1];
			name[size] = '\0';
			memcpy(name, package->name.data(), size);
			list.insert(--list.end(), name);
		}
	} exposedMods;
}

static bool loader_addAlias(const char* alias, const char* target) {
	std::string s_alias(alias);
	ShadyCore::Package::underlineToSlash(s_alias);
	std::string s_target(target);
	ShadyCore::Package::underlineToSlash(s_target);

	auto iter = ModPackage::basePackage->find(s_target);
	if (iter == ModPackage::basePackage->end()) return false;
	return ModPackage::basePackage->alias(s_alias, iter.entry()) != ModPackage::basePackage->end();
}

static int loader_addData(lua_State* L) {
	std::string alias(luaL_checkstring(L, 1));
	ShadyCore::Package::underlineToSlash(alias);

	size_t dataSize; const char* data = luaL_checklstring(L, 2, &dataSize);
	charbuf buffer(data, data+dataSize);
	std::istream input(&buffer);

	lua_pushboolean(L, ModPackage::basePackage->insert(alias, input) != ModPackage::basePackage->end());
	return 1;
}

static bool loader_removeFile(const char* alias, lua_State* L) {
	std::string s_alias(alias);
	ShadyCore::Package::underlineToSlash(s_alias);

	auto iter = ModPackage::basePackage->find(s_alias);
	if (iter == ModPackage::basePackage->end()) return false;
	auto loader = reinterpret_cast<_lua_loader*>(ShadyLua::ScriptMap[L]->userdata);
	if (iter.entry().getParent() != loader->base
		&& iter.entry().getParent() != loader->owner) {
		Logger::Error("Can't remove files from other mods.");
		return false;
	}

	return ModPackage::basePackage->erase(s_alias);
}

static int loader_underlineToSlash(lua_State* L) {
	std::string s_filename = luaL_checkstring(L, 1);
	ShadyCore::Package::underlineToSlash(s_filename);
	lua_pushstring(L, s_filename.c_str());
	return 1;
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
    _file->entry->close(file->input);
    delete file;
}

static void _lua_destroy(void* userdata) {
    delete reinterpret_cast<_lua_loader*>(userdata);
}

static int _searcher_Lua(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	auto script = ShadyLua::ScriptMap[L];
	auto result = script->load(name); // maybe add file extension and handle dot separators like python
	if (result != LUA_OK) {
		return 1; // error message is probably already on stack else 
	} else {
		// module is already on stack
		lua_pushstring(L, name);
		return 2;
	}
}
//static int _searcher_C(lua_State* L);

static void LualibLoader(lua_State* L) {
	luabridge::getGlobalNamespace(L)
		.beginNamespace("loader")
			.addFunction("addAlias", loader_addAlias)
			.addFunction("addData", loader_addData)
			.addFunction("removeFile", loader_removeFile)
			.addCFunction("underlineToSlash", loader_underlineToSlash)
		.endNamespace();
	auto p = luabridge::getGlobal(L, "package");
	p["searchers"].append(&_searcher_Lua);
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
        exposedMods.add(p);
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

extern "C" __declspec(dllexport) const char** getModsList() {
	return exposedMods.list.data();
}

extern "C" __declspec(dllexport) bool setEnabled(bool enabled, char *moduleName) {
	ModPackage* package = 0;
	{ std::shared_lock lock(ModPackage::descMutex);
	for(auto& p : ModPackage::descPackage) {
		if (p->name == moduleName) {
			package = p;
			break;
		}
	} }

	if (!package) return false;
	if (enabled && !package->isEnabled()) EnablePackage(package);
	if (!enabled && package->isEnabled()) DisablePackage(package);
	return true;
}

extern "C" __declspec(dllexport) bool canBeDisabled = true;

namespace {
	using FT = ShadyCore::FileType;

	const std::unordered_map<FT::Type, FT> typeMap = {
		{FT::TYPE_TEXT,    FT(FT::TYPE_TEXT, FT::TEXT_GAME, FT::getExtValue(".cv0"))},
		{FT::TYPE_TABLE,   FT(FT::TYPE_TABLE, FT::TABLE_GAME, FT::getExtValue(".cv1"))},
		{FT::TYPE_LABEL,   FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl"))},
		{FT::TYPE_IMAGE,   FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2"))},
		{FT::TYPE_PALETTE, FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal"))},
		{FT::TYPE_SFX,     FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3"))},
		{FT::TYPE_BGM,     FT(FT::TYPE_BGM, FT::BGM_OGG, FT::getExtValue(".ogg"))},
	};
}

extern "C" __declspec(dllexport) const char* readFileData(const char* filename, size_t& size) {
	using FT = ShadyCore::FileType;

	// normalize output format
	auto filetype = FT::get(filename);
	if (filetype.format == FT::FORMAT_UNKNOWN) switch(filetype.type) {
		case FT::TYPE_TEXT: filetype.format = FT::TEXT_GAME; break;
		case FT::TYPE_TABLE: filetype.format = FT::TABLE_GAME; break;
		case FT::TYPE_SCHEMA: {
			std::string_view fname(filename);
			if (fname.ends_with("effect.pat") || fname.ends_with("stand.pat"))
				filetype.format = FT::SCHEMA_GAME_ANIM;
			else filetype.format = FT::SCHEMA_GAME_PATTERN;
		}
	}

	// try loading from shady's loaded packages
	{ std::shared_lock lock(*ModPackage::basePackage);
		auto iter = ModPackage::basePackage->find(filename, filetype.type);
		if (iter != ModPackage::basePackage->end()) {
			auto innertype = iter.fileType();

			// if same format then passthrough otherwise convert it
			char * data = nullptr;
			if (filetype.format != innertype.format) {
				auto& input = iter.open();
				std::stringstream buffer;
				ShadyCore::convertResource(filetype.type, innertype.format, input, filetype.format, buffer);
				size = buffer.tellp();
				iter.close(input);
				if (size == 0) return 0;

				data = (char*)SokuLib::NewFct(size);
				buffer.read(data, size);
			} else {
				size = iter.entry().getSize();
				auto& buffer = iter.open();
				data = (char*)SokuLib::NewFct(size);
				buffer.read(data, size);
				iter.close(buffer);
			}

			return data;
		}
	}

	SokuLib::PackageReader reader; reader.fp = 0;
	FT::Format innerformat = FT::FORMAT_UNKNOWN;
	const char* cdot = strrchr(filename, '.');
	if (!cdot) cdot = filename + strlen(filename);

	// normalize input format from game standards and open the reader
	if (filetype.type == FT::TYPE_SCHEMA) {
		if (filetype.format != FT::SCHEMA_GAME_ANIM && filetype.format != FT::SCHEMA_GAME_PATTERN) {
			std::string innername(filename, cdot);
			FT::appendExtValue(innername, FT::getExtValue(".dat"));
			reader.open(innername.c_str());
			if (reader.fp) innerformat = FT::SCHEMA_GAME_GUI;
		}
		if (reader.fp == 0 || filetype.format != FT::SCHEMA_GAME_GUI) {
			std::string innername(filename, cdot);
			FT::appendExtValue(innername, FT::getExtValue(".pat"));
			reader.open(innername.c_str());
			if (reader.fp) if (innername.ends_with("effect.pat") || innername.ends_with("stand.pat"))
				innerformat = FT::SCHEMA_GAME_ANIM;
			else innerformat = FT::SCHEMA_GAME_PATTERN;
		}
	} else {
		auto iter = typeMap.find(filetype.type);
		if (iter == typeMap.end()) return 0;
		std::string innername(filename, cdot);
		FT::appendExtValue(innername, iter->second.extValue);
		reader.open(innername.c_str());
		innerformat = iter->second.format;
	}

	// on failure returns zero
	if (reader.fp == 0) return 0;
	if (innerformat == FT::FORMAT_UNKNOWN) { reader.close(); return 0; }

	// if same format then passthrough otherwise convert it
	if (filetype.format != innerformat) {
		size = reader.GetLength();
		char* data = new char[size];
		reader.Read(data, size);
		reader.close();

		std::stringstream output;
		{ charbuf buffer(data, data+size);
		std::istream input(&buffer);
		ShadyCore::convertResource(filetype.type, innerformat, input, filetype.format, output);
		delete[] data; }

		size = output.tellp();
		data = (char*)SokuLib::NewFct(size);
		output.read(data, size);
		return data;
	} else {
		size = reader.GetLength();
		char* data = (char*)SokuLib::NewFct(size);
		reader.Read(data, size);
		reader.close();
		return data;
	}
}
