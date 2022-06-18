#include "../lualibs.hpp"
#include "../script.hpp"
#include "../logger.hpp"
#include "../../Core/package.hpp"
#include "../../Core/dataentry.hpp"
#include "../../Core/fileentry.hpp"
#include "../../Core/util/tempfiles.hpp"
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

// TODO check if is better to receive a string_view or a string
void ShadyLua::EmitSokuEventFileLoader2(const char* filename, std::istream** input, int* size) {
	std::shared_lock guard(packageMapLock);

	for (auto& pair : packageMap) {
		auto& package = pair.second;
		auto iter = package->find(filename);
		if (iter == package->end()) continue;
		auto inputType = iter.fileType();
		auto outputType = ShadyCore::GetDataPackageDefaultType(inputType, iter->second);
		// TODO reduce copying after change the reader hook
		auto stream = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
		ShadyCore::convertResource(inputType.type,
						inputType.format, iter.open(),
						outputType.format, *stream);
		*size = iter->second->getSize();
		*input = stream;
	}
}

void ShadyLua::RemoveLoaderEvents(LuaScript* script) {
	std::unique_lock lock(packageMapLock);
	packageMap.erase(script);
}

static bool loader_addAlias(const char* alias, const char* target, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	auto package = packageMap[ShadyLua::ScriptMap[L]];

	auto iter = package->find(target);
	if (iter == package->end()) { Logger::Error("Target resource was not found."); return false; }

	return package->alias(alias, iter.entry()) != package->end();
}

static int loader_addData(lua_State* L) {
	const char* alias = luaL_checkstring(L, 1);
	size_t dataSize; const char* data = luaL_checklstring(L, 2, &dataSize);
	charbuf buffer(data, data+dataSize);
	std::istream input(&buffer);

	std::shared_lock guard(packageMapLock);
	auto package = packageMap[ShadyLua::ScriptMap[L]];
	lua_pushboolean(L, package->insert(alias, input) != package->end());
	return 1;
}

static bool loader_addResource(std::string alias, RefCountedObjectPtr<ShadyLua::ResourceProxy> proxy, lua_State* L) {
	auto outputType = ShadyCore::GetDataPackageDefaultType(proxy->type, proxy->resource);
	alias = alias.substr(0, alias.find_last_of('.')); outputType.appendExtValue(alias); // replace extension
	auto tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile, std::ios::binary);
	ShadyCore::getResourceWriter(outputType)(proxy->resource, output);

	std::shared_lock guard(packageMapLock);
	auto package = packageMap[ShadyLua::ScriptMap[L]];
	return package->insert(alias, new ShadyCore::FilePackageEntry(package, tempFile, true)) != package->end();
}

static bool loader_addFile(const char* alias, const char* filename, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	return package->insert(alias, std::filesystem::u8path(filename)) != package->end();
}

static int loader_addPackage(const char* filename, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	return (int)package->merge(std::filesystem::u8path(filename));
}

static bool loader_removeFile(const char* alias, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	return package->erase(alias);
}

static bool loader_removePackage(int childId, lua_State* L) {
	auto child = reinterpret_cast<ShadyCore::Package*>(childId);
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[ShadyLua::ScriptMap[L]]);

	child = package->demerge(child);
	if (child) delete child;
	return child;
}

void ShadyLua::LualibLoader(lua_State* L, ShadyCore::PackageEx* package) {
	packageMap[ShadyLua::ScriptMap[L]] = package;
	// TODO check lua require() behavior
	// auto package = luabridge::getGlobal(L, "package");
	// package["cpath"] = package["cpath"].tostring()
	//     + ";" + basePath.string() + "\\?.dll";
	// package["path"] = package["path"].tostring()
	//     + ";" + basePath.string() + "\\?.lua"
	//     + ";" + basePath.string() + "\\?\\init.lua"
	//     + ";" + basePath.string() + "\\lua\\?.lua"
	//     + ";" + basePath.string() + "\\lua\\?\\init.lua";

	getGlobalNamespace(L)
		.beginNamespace("loader")
			.addCFunction("addData", loader_addData)
			.addFunction("addResource", loader_addResource)
			.addFunction("addAlias", loader_addAlias)
			.addFunction("addFile", loader_addFile)
			.addFunction("addPackage", loader_addPackage)
			.addFunction("removeFile", loader_removeFile)
			.addFunction("removePackage", loader_removePackage)
		.endNamespace();
}