#include <windows.h>
#include <filesystem>
#include "script.hpp"
#include "logger.hpp"

extern const BYTE TARGET_HASH[16];
const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
std::filesystem::path modulePath;

static bool GetModulePath(HMODULE handle, std::filesystem::path& result) {
	// use wchar for better path handling
	std::wstring buffer;
	int len = MAX_PATH + 1;
	do {
		buffer.resize(len);
		len = GetModuleFileNameW(handle, buffer.data(), buffer.size());
	} while(len > buffer.size());

	if (len) result = std::filesystem::path(buffer.begin(), buffer.begin()+len).parent_path();
	return len;
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	GetModulePath(hMyModule, modulePath);

	Logger::Initialize(Logger::LOG_DEBUG | Logger::LOG_ERROR | Logger::LOG_INFO | Logger::LOG_WARNING);

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {}

/*
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
*/

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	if (fdwReason == DLL_PROCESS_DETACH) Logger::Finalize();
	return TRUE;
}
