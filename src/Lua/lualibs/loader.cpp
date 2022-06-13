#include "../lualibs.hpp"
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
	std::unordered_map<lua_State*, ShadyCore::Package*> packageMap;
	std::unordered_map<std::string, std::pair<lua_State*, int>> callbackMap;
	std::shared_mutex packageMapLock;

	struct charbuf : std::streambuf {
		charbuf(const char* begin, const char* end) {
			this->setg((char*)begin, (char*)begin, (char*)end);
		}
	};
}

// TODO check if is better to receive a string_view or a string
void ShadyLua::EmitSokuEventFileLoader2(const char* filename, std::istream** input, int* size) {
	std::shared_lock guard(packageMapLock);

	auto iter = callbackMap.find(filename);
	if (iter != callbackMap.end()) {
		lua_State* L = iter->second.first;
		lua_rawgeti(L, LUA_REGISTRYINDEX, iter->second.second);
		lua_pushstring(L, filename);
		if (lua_pcall(L, 1, 1, 0)) {
			Logger::Error(lua_tostring(L, -1));
			lua_pop(L, 1);
		} switch(lua_type(L, 1)) {
			case LUA_TSTRING: {
				size_t dataSize = 0;
				const char* data = lua_tolstring(L, 1, &dataSize);
				if (!data || !dataSize) break;
				// TODO other formats
				*size = dataSize;
				*input = new std::stringstream(std::string(data, dataSize),
					std::ios::in|std::ios::out|std::ios::binary);
			} break;
			case LUA_TUSERDATA: {
				RefCountedObjectPtr<ShadyLua::ResourceProxy> proxy = Stack<ShadyLua::ResourceProxy*>::get(L, 1);
				auto outputType = ShadyCore::GetDataPackageDefaultType(proxy->type, proxy->resource);
				std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
				ShadyCore::getResourceWriter(outputType)(proxy->resource, *buffer);
				*size = buffer->tellp();
				*input = buffer;
			} break;
		} lua_pop(L, 1);
		if (*input) return;
	}

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

static bool loader_addAlias(const char* alias, const char* target, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	auto package = packageMap[L];

	auto iter = package->find(target);
	if (iter == package->end()) { Logger::Error("Target resource was not found."); return false; }

	return package->alias(alias, iter.entry()) != package->end();
}

static int loader_addData(lua_State* L) {
	const char* alias = luaL_checkstring(L, 1);
	size_t dataSize; const char* data = luaL_checklstring(L, 2, &dataSize);

	std::filesystem::path tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile, std::ios::binary);
	output.write(data, dataSize);
	output.close();

	std::shared_lock guard(packageMapLock);
	auto package = packageMap[L];
	lua_pushboolean(L, package->insert(alias, new ShadyCore::FilePackageEntry(package, tempFile, true)) != package->end());
	return 1;
}

static int loader_addCallback(lua_State* L) {
	if (lua_gettop(L) < 2 ) return luaL_error(L, "addCallback takes 2 arguments.");
	if (!lua_isfunction(L, 2)) return luaL_error(L, "Must pass a callback");
	const char * alias = luaL_checkstring(L, 1);

	std::shared_lock guard(packageMapLock);
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);
	callbackMap[alias] = std::make_pair(L, callback);

	lua_pushinteger(L, callback);
	return 1;
}

static bool loader_addResource(std::string alias, RefCountedObjectPtr<ShadyLua::ResourceProxy> proxy, lua_State* L) {
	auto outputType = ShadyCore::GetDataPackageDefaultType(proxy->type, proxy->resource);
	alias = alias.substr(0, alias.find_last_of('.')); outputType.appendExtValue(alias); // replace extension
	auto tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile, std::ios::binary);
	ShadyCore::getResourceWriter(outputType)(proxy->resource, output);

	std::shared_lock guard(packageMapLock);
	auto package = packageMap[L];
	return package->insert(alias, new ShadyCore::FilePackageEntry(package, tempFile, true)) != package->end();
}

static bool loader_addFile(const char* alias, const char* filename, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[L]);

	return package->insert(alias, std::filesystem::u8path(filename)) != package->end();
}

static int loader_addPackage(const char* filename, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[L]);

	return (int)package->merge(std::filesystem::u8path(filename));
}

static bool loader_removeFile(const char* alias, lua_State* L) {
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[L]);

	return package->erase(alias);
}

static bool loader_removePackage(int childId, lua_State* L) {
	auto child = reinterpret_cast<ShadyCore::Package*>(childId);
	std::shared_lock guard(packageMapLock);
	ShadyCore::PackageEx* package = reinterpret_cast<ShadyCore::PackageEx*>(packageMap[L]);

	child = package->demerge(child);
	if (child) delete child;
	return child;
}

void ShadyLua::LualibLoader(lua_State* L, ShadyCore::Package* package, bool isEx) {
	packageMap[L] = package;
	// TODO check lua require() behavior
	// auto package = luabridge::getGlobal(L, "package");
	// package["cpath"] = package["cpath"].tostring()
	//     + ";" + basePath.string() + "\\?.dll";
	// package["path"] = package["path"].tostring()
	//     + ";" + basePath.string() + "\\?.lua"
	//     + ";" + basePath.string() + "\\?\\init.lua"
	//     + ";" + basePath.string() + "\\lua\\?.lua"
	//     + ";" + basePath.string() + "\\lua\\?\\init.lua";

	auto ns = getGlobalNamespace(L).beginNamespace("loader");
	ns.addCFunction("addData", loader_addData);
	ns.addFunction("addCallback", loader_addCallback);
	ns.addFunction("addResource", loader_addResource);
	if (isEx) {
		ns.addFunction("addAlias", loader_addAlias);
		ns.addFunction("addFile", loader_addFile);
		ns.addFunction("addPackage", loader_addPackage);
		ns.addFunction("removeFile", loader_removeFile);
		ns.addFunction("removePackage", loader_removePackage);
	}
	ns.endNamespace();
}