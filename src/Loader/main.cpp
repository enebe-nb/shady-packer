#include "main.hpp"
#include "modpackage.hpp"

#include <curl/curl.h>
#include <fstream>

namespace {
	const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
	EventID fileLoaderEvent;
}
bool iniAutoUpdate;
bool iniUseLoadLock;
std::mutex loadLock;
bool hasSokuEngine;

static bool GetModulePath(HMODULE handle, std::filesystem::path& result) {
	// use wchar for better path handling
	std::wstring buffer;
	int len = MAX_PATH + 1;
	do {
		buffer.resize(len);
		len = GetModuleFileNameW(handle, buffer.data(), buffer.size());
	} while(len > buffer.size());

	if (len) {
		result = std::filesystem::path(buffer).parent_path();
	}
	return len;
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	loadLock.lock();
	hasSokuEngine = GetModuleHandle(TEXT("SokuEngine")) != NULL && GetModuleHandle(TEXT("SokuEngine")) != INVALID_HANDLE_VALUE;
	if (hasSokuEngine) fileLoaderEvent = Soku::SubscribeEvent(SokuEvent::FileLoader, {FileLoaderCallback});

	GetModulePath(hMyModule, ModPackage::basePath);
	LoadSettings();
	if (!hasSokuEngine) LoadTamper();

	if (!iniUseLoadLock) loadLock.unlock();
	curl_global_init(CURL_GLOBAL_DEFAULT);
	LoadEngineMenu();

	if (iniUseLoadLock) loadLock.unlock();
	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	UnloadEngineMenu();
	if (hasSokuEngine) {
		Soku::UnsubscribeEvent(fileLoaderEvent);
	} else {
		UnloadTamper();
	}
	curl_global_cleanup();
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}

void LoadSettings() {
	iniAutoUpdate = GetPrivateProfileIntW(L"Options", L"autoUpdate",
		true, (ModPackage::basePath / L"shady-loader.ini").c_str());
	iniUseLoadLock = GetPrivateProfileIntW(L"Options", L"useLoadLock",
		true, (ModPackage::basePath / L"shady-loader.ini").c_str());
}

void SaveSettings() {
	WritePrivateProfileStringW(L"Options", L"autoUpdate",
		iniAutoUpdate ? L"1" : L"0", (ModPackage::basePath / L"shady-loader.ini").c_str());
	WritePrivateProfileStringW(L"Options", L"useLoadLock",
		iniUseLoadLock ? L"1" : L"0", (ModPackage::basePath / L"shady-loader.ini").c_str());

	nlohmann::json root;
	for (auto& package : ModPackage::packageList) {
		root[package->name.string()] = package->data;
		if (package->fileExists) WritePrivateProfileStringW(L"Packages", package->name.c_str(),
			package->enabled ? L"1" : L"0", (ModPackage::basePath / L"shady-loader.ini").c_str());
	}

	std::ofstream output(ModPackage::basePath / L"packages.json");
	output << root;
}