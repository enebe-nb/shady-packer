#include "main.hpp"

#include <shlwapi.h>
#include <curl/curl.h>

extern const BYTE TARGET_HASH[16];
const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
std::string modulePath;
struct IniConfig iniConfig;

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	char tmpPath[MAX_PATH];
	GetModuleFileNameA(hMyModule, tmpPath, MAX_PATH);
	PathRemoveFileSpecA(tmpPath);
	modulePath = tmpPath;

	curl_global_init(CURL_GLOBAL_DEFAULT);
	LoadConverter();
	LoadSettings();
	if (iniConfig.useIntercept) LoadIntercept();
	LoadEngineMenu();

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	UnloadEngineMenu();
	if (iniConfig.useIntercept) UnloadIntercept();
	curl_global_cleanup();
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
