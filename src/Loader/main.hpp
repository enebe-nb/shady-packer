#pragma once

#include <filesystem>
#include <shared_mutex>
#include <string>
#include <IFileReader.hpp>

extern bool iniAutoUpdate;
extern bool iniUseLoadLock;
extern std::string iniRemoteConfig;

void LoadSettings();
void SaveSettings();
void LoadPackage();
void UnloadPackage();
void HookLoader(const std::wstring& caller);
void UnloadLoader();
