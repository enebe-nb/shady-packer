#include "main.hpp"
#include "modpackage.hpp"

#include <shlwapi.h>
#include <curl/curl.h>
#include <fstream>

namespace {
	const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
	EventID fileLoaderEvent;
}
struct IniConfig iniConfig;
std::mutex loadLock;
bool hasSokuEngine;

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	loadLock.lock();
	hasSokuEngine = GetModuleHandle(TEXT("SokuEngine")) != NULL && GetModuleHandle(TEXT("SokuEngine")) != INVALID_HANDLE_VALUE;
	if (hasSokuEngine) fileLoaderEvent = Soku::SubscribeEvent(SokuEvent::FileLoader, {FileLoaderCallback});

	TCHAR tmpPath[MAX_PATH];
	GetModuleFileName(hMyModule, tmpPath, MAX_PATH);
	ModPackage::basePath = std::filesystem::path(tmpPath).parent_path();

	LoadSettings();
	if (hasSokuEngine) {
		if (iniConfig.useIntercept) LoadIntercept();
		else LoadConverter();
	} else {
		LoadTamper();
	}

	if (!iniConfig.useLoadLock) loadLock.unlock();
	curl_global_init(CURL_GLOBAL_DEFAULT);
	LoadEngineMenu();

	if (iniConfig.useLoadLock) loadLock.unlock();
	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	UnloadEngineMenu();
	if (hasSokuEngine) {
		Soku::UnsubscribeEvent(fileLoaderEvent);
		if (iniConfig.useIntercept) UnloadIntercept();
	} else {
		UnloadTamper();
	}
	curl_global_cleanup();
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}

void LoadSettings() {
	iniConfig.useIntercept = GetPrivateProfileIntA("Options", "useIntercept",
		false, (ModPackage::basePath / "shady-loader.ini").string().c_str());
	iniConfig.autoUpdate = GetPrivateProfileIntA("Options", "autoUpdate",
		true, (ModPackage::basePath / "shady-loader.ini").string().c_str());
	iniConfig.useLoadLock = GetPrivateProfileIntA("Options", "useLoadLock",
		true, (ModPackage::basePath / "shady-loader.ini").string().c_str());
}

void SaveSettings() {
	WritePrivateProfileStringA("Options", "useIntercept",
		iniConfig.useIntercept ? "1" : "0", (ModPackage::basePath / "shady-loader.ini").string().c_str());
	WritePrivateProfileStringA("Options", "autoUpdate",
		iniConfig.autoUpdate ? "1" : "0", (ModPackage::basePath / "shady-loader.ini").string().c_str());
	WritePrivateProfileStringA("Options", "useLoadLock",
		iniConfig.useLoadLock ? "1" : "0", (ModPackage::basePath / "shady-loader.ini").string().c_str());

	nlohmann::json root;
	for (auto& package : ModPackage::packageList) {
		root[package->name.string()] = package->data;
		if (package->fileExists) WritePrivateProfileStringA("Packages", package->name.string().c_str(),
			package->enabled ? "1" : "0", (ModPackage::basePath / "shady-loader.ini").string().c_str());
	}

	std::ofstream output(ModPackage::basePath / "packages.json");
	output << root;
}