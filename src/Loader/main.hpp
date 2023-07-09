#pragma once

#include <filesystem>
#include <shared_mutex>
#include <string>
#include <IFileReader.hpp>

inline std::string ws2utf(const std::wstring_view& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

inline std::wstring utf2ws(const std::string_view& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

extern bool iniAutoUpdate;
extern bool iniUseLoadLock;
extern bool iniEnableLua;
extern std::string iniRemoteConfig;

void LoadSettings();
void SaveSettings();
void LoadPackage();
void UnloadPackage();
void HookLoader();
void UnhookLoader();
