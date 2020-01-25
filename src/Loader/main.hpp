#pragma once
#include <fstream>
#include <windows.h>
#include <mutex>
#include <Sokulib.h>

extern std::string modulePath;
extern struct IniConfig {
    bool useIntercept;
    bool autoUpdate;
    bool useLoadLock;
} iniConfig;
extern std::mutex loadLock;

void LoadSettings();
void SaveSettings();
void LoadEngineMenu();
void UnloadEngineMenu();
void LoadIntercept();
void UnloadIntercept();
void LoadConverter();
void decodeHtml(std::string& data);
void FileLoaderCallback(SokuData::FileLoaderData& data);

inline std::string ws2s(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

inline std::wstring s2ws(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
