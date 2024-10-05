#include <windows.h>
#include <filesystem>
#include "script.hpp"
#include "logger.hpp"
#include "lualibs/soku.hpp"
#include "lualibs.hpp"
#include "../Core/util/filewatcher.hpp"
#include <LuaBridge/LuaBridge.h>

void* ShadyCore::Allocate(size_t s) { return SokuLib::NewFct(s); }
void ShadyCore::Deallocate(void* p) { SokuLib::DeleteFct(p); }

const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
std::filesystem::path modulePath;
namespace {
    std::unordered_map<ShadyUtil::FileWatcher*, std::pair<ShadyLua::LuaScript*, std::filesystem::path>> watchers;
}

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

static inline ShadyLua::LuaScript* startScript(const std::filesystem::path& scriptPath) {
    const auto basePath = scriptPath.parent_path();
    auto script = new ShadyLua::LuaScriptFS(basePath);
    auto package = luabridge::getGlobal(script->L, "package");
    package["cpath"] = package["cpath"].tostring()
        + ";" + modulePath.string() + "\\libs\\?.dll"
        + ";" + basePath.string() + "\\?.dll";
    package["path"] = package["path"].tostring()
        + ";" + modulePath.string() + "\\libs\\?.lua"
        + ";" + modulePath.string() + "\\libs\\?\\init.lua"
        + ";" + basePath.string() + "\\?.lua"
        + ";" + basePath.string() + "\\?\\init.lua";
    if (script->load((const char*)scriptPath.filename().u8string().c_str()) == LUA_OK) script->run();
    return script;
}

static void LoadSettings() {
    wchar_t buffer[32767];
    int len = GetPrivateProfileSectionW(L"Scripts", buffer, 32767, (modulePath / L"shady-lua.ini").c_str());
    if (!len) return;

    wchar_t* line = buffer;
    while(len = wcslen(line)) {
        std::filesystem::path scriptPath(wcschr(line, L'=') + 1);
        scriptPath = modulePath / scriptPath;
        auto script = startScript(scriptPath);
        watchers[ShadyUtil::FileWatcher::create(scriptPath)] = std::make_pair(script, scriptPath);
        line += len + 1;
    }
}

static void __fastcall onProcessHook_replFn(void* sceneManager);
using onProcessHook = ShadyLua::AddressHook<0x00861ae8, onProcessHook_replFn>;
onProcessHook::typeFn onProcessHook::origFn = reinterpret_cast<onProcessHook::typeFn>(0x0041e240);
static void __fastcall onProcessHook_replFn(void* sceneManager) {
    onProcessHook::origFn(sceneManager);

    ShadyUtil::FileWatcher* current;
    while(current = ShadyUtil::FileWatcher::getNextChange()) {
        auto& script = watchers[current];

        switch (current->action) {
        case ShadyUtil::FileWatcher::CREATED:
        case ShadyUtil::FileWatcher::MODIFIED:
            if (script.first) delete script.first;
            script.first = startScript(script.second);
            break;
        case ShadyUtil::FileWatcher::REMOVED:
            if (script.first) delete script.first;
            script.first = nullptr;
            break;
        case ShadyUtil::FileWatcher::RENAMED:
            // do nothing, the watcher changes to the new filename
            break;
        }
    }
}

static bool __fastcall onRenderHook_replFn(void* renderer);
using onRenderHook = ShadyLua::CallHook<0x0043dda6, onRenderHook_replFn>;
onRenderHook::typeFn onRenderHook::origFn = reinterpret_cast<onRenderHook::typeFn>(0x00444790);
static bool __fastcall onRenderHook_replFn(void* renderer) {
    bool ret = onRenderHook::origFn(renderer);
    Logger::Render();
    return ret;
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	GetModulePath(hMyModule, modulePath);
#ifdef _DEBUG
    Logger::Initialize(Logger::LOG_ALL, "shady-lua.log");
#else
    Logger::Initialize(Logger::LOG_ALL);
#endif
    ShadyLua::initHook<onProcessHook>();
    ShadyLua::initHook<onRenderHook>();

    LoadSettings();

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	if (fdwReason == DLL_PROCESS_DETACH) Logger::Finalize();
	return TRUE;
}
