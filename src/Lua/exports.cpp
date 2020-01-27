#include <windows.h>
#include <shlwapi.h>
#include <zip.h>
#include "script.hpp"
#include "logger.hpp"

extern const BYTE TARGET_HASH[16];
const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
std::string modulePath;

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	char tmpPath[MAX_PATH];
	GetModuleFileName(hMyModule, tmpPath, MAX_PATH);
	PathRemoveFileSpec(tmpPath);
	modulePath = tmpPath;

	Logger::Initialize(Logger::LOG_DEBUG | Logger::LOG_ERROR | Logger::LOG_INFO | Logger::LOG_WARNING);

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	Logger::Finalize();
}

extern "C" __declspec(dllexport) ShadyLua::LuaScript* LoadFromGeneric(void* userdata, ShadyLua::fnOpen_t open, ShadyLua::fnRead_t read, const char* filename) {
    ShadyLua::LuaScript* script = new ShadyLua::LuaScript(userdata, open, read);
    if (script->load(filename) != LUA_OK || !script->run()) {
        delete script;
        return 0;
    } return script;
}

extern "C" __declspec(dllexport) ShadyLua::LuaScript* LoadFromFilesystem(const char* path) {
    const char* filename = PathFindFileName(path);
    ShadyLua::LuaScriptFS* script = new ShadyLua::LuaScriptFS(std::string(path, filename));
    if (script->load(filename) != LUA_OK || !script->run()) {
        delete script;
        return 0;
    } return script;
}

extern "C" __declspec(dllexport) ShadyLua::LuaScript* LoadFromZip(const char* zipFile, const char* filename) {
    ShadyLua::LuaScriptZip* script = new ShadyLua::LuaScriptZip(zipFile);
    if (script->load(filename) != LUA_OK || !script->run()) {
        delete script;
        return 0;
    } return script;
}

extern "C" __declspec(dllexport) void FreeScript(ShadyLua::LuaScript* script) {
	delete script;
}

extern "C" __declspec(dllexport) bool ZipExists(const char* zipFile, const char* filename) {
    zip_t* zip = zip_open(zipFile, ZIP_RDONLY, 0);
    zip_int64_t index = zip_name_locate(zip, filename, ZIP_FL_NOCASE);
    zip_close(zip);
	return index >= 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
