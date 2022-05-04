#pragma once

#include <filesystem>
#include <shared_mutex>
#include <string>
#include <IFileReader.hpp>

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
