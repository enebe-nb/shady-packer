#pragma once
#include <fstream>

extern std::ofstream outlog;
extern std::string modulePath;

void LoadSettings();
void SaveSettings();
void LoadEngineMenu();
void UnloadEngineMenu();
void LoadFileLoader();
void UnloadFileLoader();