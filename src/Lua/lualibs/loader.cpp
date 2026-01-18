#include "../lualibs.hpp"
#include "../script.hpp"
#include "../logger.hpp"
#include "../../Core/package.hpp"
#include "../../Core/dataentry.hpp"
#include "../../Core/fileentry.hpp"
#include "../../Core/util/tempfiles.hpp"
#include "../../Core/reader.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedPtr.h>
#include <unordered_map>

using namespace luabridge;

namespace {
	std::unordered_map<ShadyLua::LuaScript*, ShadyCore::PackageEx*> packageMap;
	std::shared_mutex packageMapLock;

	struct charbuf : std::streambuf {
		inline charbuf(const char* begin, const char* end) {
			this->setg((char*)begin, (char*)begin, (char*)end);
		}
	};
}

// ---- LoaderHook ---
static void* __stdcall LoaderHook_replFn(const char* filename, unsigned int* _size, unsigned int* _offset);
using LoaderHook = ShadyLua::CallHook<0x0040d227, LoaderHook_replFn>;
LoaderHook::typeFn LoaderHook::origFn = reinterpret_cast<LoaderHook::typeFn>(0x0041c080);
static void* __stdcall LoaderHook_replFn(const char* filename, unsigned int* _size, unsigned int* _offset) {
	int esi_value;
	__asm mov esi_value, esi

	{ std::shared_lock guard(packageMapLock);
		for (auto& pair : packageMap) {
			auto& package = pair.second;
			auto iter = package->find(filename);
			if (iter == package->end()) continue;
			auto inputType = iter.fileType();
			auto outputType = ShadyCore::GetDataPackageDefaultType(inputType, iter->second);
			// TODO reduce copying
			auto stream = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
			auto& input = iter.open();
			ShadyCore::convertResource(inputType.type,
							inputType.format, input,
							outputType.format, *stream);
			iter.close(input);
			*_size = iter->second->getSize();
			*_offset = 0x40000000; // just to hold a value
			*(int*)esi_value = ShadyCore::stream_reader_vtbl;
			return stream;
		}
	}

	// return __readerCreate(filename, _size, _offset);
	__asm {
		push _offset;
		push _size;
		push filename;
		mov esi, esi_value;
		call LoaderHook::origFn;
		mov esi_value, eax;
	} return (void*)esi_value;
}

void ShadyLua::RemoveLoaderEvents(LuaScript* script) {
	std::unique_lock lock(packageMapLock);
	packageMap.erase(script);
}

static bool loader_addAlias(const char* alias, const char* target, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	auto package = packageMap[ShadyLua::ScriptMap[L]];

	std::string s_alias(alias);
	ShadyCore::Package::underlineToSlash(s_alias);
	std::string s_target(target);
	ShadyCore::Package::underlineToSlash(s_target);

	auto iter = package->find(s_target);
	if (iter == package->end()) { Logger::Error("Target resource was not found."); return false; }

	return package->alias(s_alias, iter.entry()) != package->end();
}

static int loader_addData(lua_State* L) {
	const char* alias = luaL_checkstring(L, 1);
	std::string s_alias(alias);
	ShadyCore::Package::underlineToSlash(s_alias);

	size_t dataSize; const char* data = luaL_checklstring(L, 2, &dataSize);
	charbuf buffer(data, data+dataSize);
	std::istream input(&buffer);

	std::shared_lock guard(packageMapLock);
	auto package = packageMap[ShadyLua::ScriptMap[L]];
	lua_pushboolean(L, package->insert(s_alias, input) != package->end());
	return 1;
}

static bool loader_addResource(std::string alias, RefCountedObjectPtr<ShadyLua::ResourceProxy> proxy, lua_State* L) {
	auto outputType = ShadyCore::GetDataPackageDefaultType(proxy->type, proxy->resource);
	ShadyCore::Package::underlineToSlash(alias);
	alias = alias.substr(0, alias.find_last_of('.')); outputType.appendExtValue(alias); // replace extension
	auto tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile, std::ios::binary);
	ShadyCore::getResourceWriter(outputType)(proxy->resource, output);

	std::shared_lock guard(packageMapLock);
	auto package = packageMap[ShadyLua::ScriptMap[L]];
	return package->insert(alias, new ShadyCore::FilePackageEntry(package, tempFile, true)) != package->end();
}

static bool loader_addFile(const char* alias, const char* filename, lua_State* L) {
	std::string s_alias(alias);
	ShadyCore::Package::underlineToSlash(s_alias);

	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	return package->insert(s_alias, std::filesystem::u8path(filename)) != package->end();
}

static int loader_addPackage(const char* filename, lua_State* L) {
	std::string s_filename(filename);
	ShadyCore::Package::underlineToSlash(s_filename);

	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	return (int)package->merge(std::filesystem::u8path(s_filename));
}

static bool loader_removeFile(const char* alias, lua_State* L) {
	std::string s_alias(alias);
	ShadyCore::Package::underlineToSlash(s_alias);

	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	return package->erase(s_alias);
}

static bool loader_removePackage(int childId, lua_State* L) {
	auto child = reinterpret_cast<ShadyCore::Package*>(childId);
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	child = package->demerge(child);
	if (child) delete child;
	return child;
}

static int loader_underlineToSlash(lua_State* L) {
	std::string s_filename = luaL_checkstring(L, 1);
	ShadyCore::Package::underlineToSlash(s_filename);
	lua_pushstring(L, s_filename.c_str());
	return 1;
}

void ShadyLua::LualibLoader(lua_State* L, ShadyCore::PackageEx* package) {
	packageMap[ShadyLua::ScriptMap[L]] = package;
	initHook<LoaderHook>();

	getGlobalNamespace(L)
		.beginNamespace("loader")
			.addConstant("basePath", package->getBasePath().string())
			.addCFunction("addData", loader_addData)
			.addFunction("addResource", loader_addResource)
			.addFunction("addAlias", loader_addAlias)
			.addFunction("addFile", loader_addFile)
			.addFunction("addPackage", loader_addPackage)
			.addFunction("removeFile", loader_removeFile)
			.addFunction("removePackage", loader_removePackage)
			.addCFunction("underlineToSlash", loader_underlineToSlash)
		.endNamespace();
}