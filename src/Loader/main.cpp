#include "main.hpp"
#include "modpackage.hpp"
#include "../Lua/logger.hpp"

namespace {
	const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
	bool _initialized = false, _initialized2 = false;
}
bool iniAutoUpdate;
bool iniUseLoadLock;
std::string iniRemoteConfig;
std::shared_mutex loadLock;

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

static bool GetModuleName(HMODULE handle, std::filesystem::path& result) {
	// use wchar for better path handling
	std::wstring buffer;
	int len = MAX_PATH + 1;
	do {
		buffer.resize(len);
		len = GetModuleFileNameW(handle, buffer.data(), buffer.size());
	} while(len > buffer.size());

	if (len) result = std::filesystem::path(buffer.begin(), buffer.begin()+len).filename();
	return len;
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	if (!_initialized) _initialized = true;
	else return FALSE;

	Logger::Initialize(Logger::LOG_ERROR);
	GetModulePath(hMyModule, ModPackage::basePath);
	LoadSettings();

	if (iniUseLoadLock) loadLock.lock();
	LoadPackage();
	std::filesystem::path callerName;
	GetModuleName(hParentModule, callerName);
	HookLoader(callerName);
	if (iniUseLoadLock) loadLock.unlock();

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	if (fdwReason == DLL_PROCESS_DETACH) {
		UnloadLoader();
		UnloadPackage();
		Logger::Finalize();
	}

	return TRUE;
}

void LoadSettings() {
	iniAutoUpdate = GetPrivateProfileIntW(L"Options", L"autoUpdate",
		true, (ModPackage::basePath / L"shady-loader.ini").c_str());
	iniUseLoadLock = GetPrivateProfileIntW(L"Options", L"useLoadLock",
		true, (ModPackage::basePath / L"shady-loader.ini").c_str());
	iniRemoteConfig.resize(64);
	auto len = GetPrivateProfileStringA("Options", "remoteConfig",
		"1EpxozKDE86N3Vb8b4YIwx798J_YfR_rt", iniRemoteConfig.data(), 64,
		(ModPackage::basePath / L"shady-loader.ini").string().c_str());
	iniRemoteConfig.resize(len);
}

void SaveSettings() {
	WritePrivateProfileStringW(L"Options", L"autoUpdate",
		iniAutoUpdate ? L"1" : L"0", (ModPackage::basePath / L"shady-loader.ini").c_str());
	WritePrivateProfileStringW(L"Options", L"useLoadLock",
		iniUseLoadLock ? L"1" : L"0", (ModPackage::basePath / L"shady-loader.ini").c_str());
	WritePrivateProfileStringA("Options", "remoteConfig",
		iniRemoteConfig.c_str(), (ModPackage::basePath / L"shady-loader.ini").string().c_str());

	nlohmann::json root;
	ModPackage::packageListMutex.lock_shared();
	for (auto& package : ModPackage::packageList) {
		root[package->name.string()] = package->data;
		if (package->fileExists) WritePrivateProfileStringW(L"Packages", package->name.c_str(),
			package->enabled ? L"1" : L"0", (ModPackage::basePath / L"shady-loader.ini").c_str());
	} ModPackage::packageListMutex.unlock_shared();

	std::ofstream output(ModPackage::basePath / L"packages.json");
	output << root;
}