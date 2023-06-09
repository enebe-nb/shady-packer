#include <windows.h>
#include <filesystem>
#include "script.hpp"
#include "logger.hpp"
#include "lualibs/soku.hpp"
#include "lualibs.hpp"

namespace ShadyCore {
    void* Allocate(size_t s) { return new uint8_t[s]; }
    void Deallocate(void* p) { delete[] (uint8_t*)p; }
}

extern const BYTE TARGET_HASH[16];
const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
std::filesystem::path modulePath;
namespace {
    bool iniShowConsole;
    ShadyLua::LuaScript* consoleScript = 0;
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

static void LoadSettings() {
	iniShowConsole = GetPrivateProfileIntW(L"Options", L"showConsole",
		false, (modulePath / L"shady-lua.ini").c_str());

    wchar_t buffer[32767];
    int len = GetPrivateProfileSectionW(L"Scripts", buffer, 32767, (modulePath / L"shady-lua.ini").c_str());
    if (!len) return;

    wchar_t* line = buffer;
    while(len = wcslen(line)) {
        std::filesystem::path scriptPath(wcschr(line, L'=') + 1);
        scriptPath = modulePath / scriptPath;
        auto script = new ShadyLua::LuaScriptFS(scriptPath.parent_path());
        if (script->load((const char*)scriptPath.filename().u8string().c_str()) == LUA_OK) script->run();
        line += len + 1;
    }
}


/* TODO redo console
static void RenderConsole() {
    static bool showConsole = iniShowConsole;
    static std::string input;
    bool didEnter = false;

    if (showConsole) { if (ImGui::Begin("Console", &showConsole, ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImVec2 content = ImGui::GetContentRegionAvail();
        ImVec2 inputSize = ImGui::CalcTextSize(input.data(), input.data() + input.size(), false, content.x - 16);
        if (ImGui::BeginChild("Log", ImVec2(content.x, content.y - inputSize.y - 16))) {
            Logger::Render();
        } ImGui::EndChild();
        if (ImGui::BeginChild("Input", ImVec2(content.x, inputSize.y + 8))) {
            didEnter = ImGui::InputTextMultiline("", &input, ImGui::GetContentRegionAvail(),
                ImGuiInputTextFlags_AllowTabInput | 
                ImGuiInputTextFlags_CtrlEnterForNewLine |
                ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_AutoSelectAll);
            if (didEnter) ImGui::SetKeyboardFocusHere(-1);
        } ImGui::EndChild();
    } ImGui::End(); }

    if (didEnter) {
        lua_State* L = consoleScript->L;
        std::lock_guard guard(consoleScript->mutex);
        if(luaL_dostring(L, input.c_str())) {
            Logger::Error(lua_tostring(L, -1));
		    lua_pop(L, 1);
        } else {
            input.clear();
            int top = lua_gettop(L);
            for(int i = 1; i <= top; ++i) {
                Logger::Debug("  Ret: ", luaL_tolstring(L, i, 0));
            }
            lua_pop(L, lua_gettop(L));
        }
    }

    ShadyLua::EmitSokuEventRender();
}

static void ResetConsole() {
    if (consoleScript) delete consoleScript;
    consoleScript = new ShadyLua::LuaScriptFS(modulePath);
    ShadyLua::LualibBase(consoleScript->L, modulePath);
    ShadyLua::LualibMemory(consoleScript->L);
    ShadyLua::LualibResource(consoleScript->L);
    ShadyLua::LualibSoku(consoleScript->L);
}
*/

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	GetModulePath(hMyModule, modulePath);
    Logger::Initialize(Logger::LOG_DEBUG | Logger::LOG_ERROR, "shady-lua.log");

    LoadSettings();

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	if (fdwReason == DLL_PROCESS_DETACH) Logger::Finalize();
	return TRUE;
}
