#include "main.hpp"
#include "modpackage.hpp"
#include "../Lua/logger.hpp"

#include <windows.h>
#include <fstream>

namespace {
	const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
	bool _initialized = false, _initialized2 = false;
}
bool iniAutoUpdate;
bool iniUseLoadLock;
bool iniEnableLua;
std::string iniRemoteConfig;

static bool GetModulePath(HMODULE handle, std::filesystem::path& result) {
	std::wstring buffer;
	int len = MAX_PATH + 1;
	do {
		buffer.resize(len);
		len = GetModuleFileNameW(handle, buffer.data(), buffer.size());
	} while(len > buffer.size());

	if (len) result = std::filesystem::path(buffer.begin(), buffer.end()).parent_path();
	return len;
}

static bool GetModuleName(HMODULE handle, std::wstring& result) {
	int len = MAX_PATH + 1;
	do {
		result.resize(len);
		len = GetModuleFileNameW(handle, result.data(), result.size());
	} while(len > result.size());
	result.resize(len);

	return len;
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	if (!_initialized) _initialized = true;
	else return FALSE;

#ifdef _DEBUG
	Logger::Initialize(Logger::LOG_ALL);
#else
	Logger::Initialize(Logger::LOG_ERROR);
#endif
	GetModulePath(hMyModule, ModPackage::basePath);
	ModPackage::basePackage.reset(new ShadyCore::PackageEx(ModPackage::basePath));
	LoadSettings();

	if (iniUseLoadLock) {
		LoadPackage();
	} else {
		std::thread t(LoadPackage);
		t.detach();
	}

	HookLoader();
	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	if (fdwReason == DLL_PROCESS_DETACH) {
		UnhookLoader();
		UnloadPackage();
		Logger::Finalize();
	}

	return TRUE;
}

void LoadSettings() {
	const std::string ipath = (ModPackage::basePath / L"shady-loader.ini").string();

	iniAutoUpdate = GetPrivateProfileIntA("Options", "autoUpdate", true, ipath.c_str());
	iniUseLoadLock = GetPrivateProfileIntA("Options", "useLoadLock", false, ipath.c_str());
	iniEnableLua = GetPrivateProfileIntA("Options", "enableLua", true, ipath.c_str());
	iniRemoteConfig.resize(64);
	size_t len = GetPrivateProfileStringA("Options", "remoteConfig",
		"1EpxozKDE86N3Vb8b4YIwx798J_YfR_rt", iniRemoteConfig.data(), 64, ipath.c_str());
	iniRemoteConfig.resize(len);
}

void SaveSettings() {
	const std::string ipath = (ModPackage::basePath / L"shady-loader.ini").string();

	WritePrivateProfileStringA("Options", "autoUpdate", iniAutoUpdate ? "1" : "0", ipath.c_str());
	WritePrivateProfileStringA("Options", "useLoadLock", iniUseLoadLock ? "1" : "0", ipath.c_str());
	WritePrivateProfileStringA("Options", "enableLua", iniEnableLua ? "1" : "0", ipath.c_str());
	WritePrivateProfileStringA("Options", "remoteConfig", iniRemoteConfig.c_str(), ipath.c_str());

	nlohmann::json root;
	{ std::shared_lock lock(ModPackage::descMutex);
	for (auto& package : ModPackage::descPackage) {
		root[package->name] = package->data;
		if (package->fileExists)
			WritePrivateProfileStringA("Packages", package->name.c_str(), package->package ? "1" : "0", ipath.c_str());
	} }

	std::ofstream output(ModPackage::basePath / L"packages.json");
	output << root;
}