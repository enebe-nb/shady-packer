#include "main.hpp"
#include "modpackage.hpp"
#include "../Lua/logger.hpp"
#include "th123intl.hpp"

#include <windows.h>
#include <fstream>
#include <SokuLib.hpp>
#include "menu.hpp"

namespace {
	const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };
	bool _initialized = false, _initialized2 = false;
	auto __onRender = reinterpret_cast<bool (__fastcall *)(void* renderer)>(0x00444790);
}
bool iniAutoUpdate;
bool iniUseLoadLock;
bool iniEnableLua;
std::string iniRemoteConfig;

void activate_menu(){
	SokuLib::activateMenu(new ModMenu());
}

extern "C" __declspec(dllexport) void InitializeMainMenu(char* name, void(**on_activate)()) {
	strcpy(name,"Loader");
	*on_activate = activate_menu;
}

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

static bool __fastcall onRender(void* renderer) {
    bool ret = __onRender(renderer);
    Logger::Render();
    return ret;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	if (!_initialized) _initialized = true;
	else return FALSE;

#ifdef _DEBUG
	Logger::Initialize(Logger::LOG_ALL, "shady-loader.log");
#else
	Logger::Initialize(Logger::LOG_ERROR | Logger::LOG_WARNING);
#endif
	DWORD prot; VirtualProtect((LPVOID)0x0043dda6, 5, PAGE_EXECUTE_WRITECOPY, &prot);
	__onRender = SokuLib::TamperNearJmpOpr(0x0043dda6, onRender);
	VirtualProtect((LPVOID)0x0043dda6, 5, prot, &prot);

	GetModulePath(hMyModule, ModPackage::basePath);
	ModPackage::basePackage.reset(new ShadyCore::PackageEx(ModPackage::basePath));
	LoadSettings();
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
	const std::filesystem::path ipath = ModPackage::basePath / L"shady-loader.ini";

	iniAutoUpdate = GetPrivateProfileIntW(L"Options", L"autoUpdate", true, ipath.c_str());
	iniUseLoadLock = GetPrivateProfileIntW(L"Options", L"useLoadLock", false, ipath.c_str());
	iniEnableLua = GetPrivateProfileIntW(L"Options", L"enableLua", true, ipath.c_str());
	wchar_t buffer[2048];
	size_t len = GetPrivateProfileStringW(L"Options", L"remoteConfig",
			L"http://shady.pinkysmile.fr/config.json", buffer, 2048, ipath.c_str());
	th123intl::ConvertCodePage(buffer, CP_UTF8, iniRemoteConfig);
}

void SaveSettings() {
	const std::filesystem::path ipath = ModPackage::basePath / L"shady-loader.ini";
	std::wstring tmpstr; th123intl::ConvertCodePage(CP_UTF8, iniRemoteConfig, tmpstr);

	WritePrivateProfileStringW(L"Options", L"autoUpdate", iniAutoUpdate ? L"1" : L"0", ipath.c_str());
	WritePrivateProfileStringW(L"Options", L"useLoadLock", iniUseLoadLock ? L"1" : L"0", ipath.c_str());
	WritePrivateProfileStringW(L"Options", L"enableLua", iniEnableLua ? L"1" : L"0", ipath.c_str());
	WritePrivateProfileStringW(L"Options", L"remoteConfig", tmpstr.c_str(), ipath.c_str());

	nlohmann::json root;
	std::wstringstream order;
	{ std::shared_lock lock(ModPackage::descMutex);
	for (auto& package : ModPackage::descPackage) {
		root[package->name] = package->data;
		if (package->fileExists) {
			std::wstring packageName;
			th123intl::ConvertCodePage(CP_UTF8, package->name, packageName);
			order << packageName.c_str() << ",";
			if (packageName[0] == L'[') packageName.insert(packageName.begin(), L'<');
			WritePrivateProfileStringW(L"Packages", packageName.c_str(), package->package ? L"1" : L"0", ipath.c_str());
		}
	} }
	WritePrivateProfileStringW(L"Options", L"order", order.str().c_str(), ipath.c_str());

	std::ofstream output(ModPackage::basePath / L"packages.json");
	output << root;
}