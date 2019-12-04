#include <shlwapi.h>
#include <SokuLib.h>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>

namespace {
	std::ofstream outlog;
	char basePath[1024];
	std::map<std::string, std::vector<ItemID> > packageList;
	std::vector<std::string> optionList;
	ItemID LoadMenuID;
}

std::wstring s2ws(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void LoadSettings() {
	std::string configPath(basePath);
	configPath += "/shady-loader.cfg";

	std::ifstream config(configPath);
	if (!config.fail()) {
		std::string line;
		while(std::getline(config, line)) if (line.size() && line[0] != '#') {
			std::string path = basePath;
			path += "/"; path += line; 
			packageList[line].push_back(Soku::AddFile(s2ws(path)));
			optionList.push_back(line);
		}
	}

	WIN32_FIND_DATA findData;
	HANDLE hFind;
	if ((hFind = FindFirstFile((std::string(basePath) + "/*").c_str(), &findData)) != INVALID_HANDLE_VALUE) {
		do {
			std::string filename(findData.cFileName);
			if (findData.cFileName[0] == '.'
				|| !(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				&& (filename.size() < 4 || filename.compare(filename.size() - 4, 4, ".zip") != 0)) continue;

			if (std::find(optionList.begin(), optionList.end(), filename) == optionList.end()) {
				optionList.push_back(filename);
			}
		} while(FindNextFile(hFind, &findData));
		FindClose(hFind);
	}

	std::sort(optionList.begin(), optionList.end());
	optionList.erase(std::unique(optionList.begin(), optionList.end()), optionList.end());
}

void SaveSettings() {
	std::string configPath(basePath);
	configPath += "/shady-loader.cfg";

	std::ofstream config(configPath);
	for(auto& pair: packageList) {
		config << pair.first << std::endl;
	}
}

void LoadMenuCallback() {
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 4));
	if (optionList.size() == 0) {
		ImGui::Text("No packages found! Put them in the shady-loader folder.");
	}

	for (auto& pack : optionList) {
		ImGui::Dummy(ImVec2(6, 20));
		ImGui::SameLine();

		bool isActive = packageList.count(pack) != 0;
		bool wasActive = isActive;
		if (ImGui::Checkbox(pack.c_str(), &isActive)) {
			if (isActive) {
				std::string path = basePath;
				path += "/"; path += pack;
				unsigned int  attr = GetFileAttributes(path.c_str());
				if (attr != INVALID_FILE_ATTRIBUTES)
					if (attr & FILE_ATTRIBUTE_DIRECTORY) {
						WIN32_FIND_DATA findData;
						HANDLE hFind;
						if ((hFind = FindFirstFile((path + "/*").c_str(), &findData)) != INVALID_HANDLE_VALUE) {
							do {
								packageList[pack].push_back(Soku::AddFile(s2ws(path + "/" + findData.cFileName)));
							} while(FindNextFile(hFind, &findData));
							FindClose(hFind);
						}
					} else {
						packageList[pack].push_back(Soku::AddFile(s2ws(path)));
					}
			} else {
				for(auto& id : packageList[pack]) Soku::RemoveFile(id);
				packageList.erase(pack);
			}

			SaveSettings();
		}
	}
	ImGui::PopStyleVar();
}

extern const BYTE TARGET_HASH[16];
const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	outlog.open("output.log");
	::GetModuleFileNameA(hMyModule, basePath, sizeof basePath);
	::PathRemoveFileSpecA(basePath);
	LoadSettings();

	LoadMenuID = Soku::AddItem(SokuComponent::EngineMenu, "Package Load", (void(*)(void)) LoadMenuCallback);

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	outlog.close();
	Soku::RemoveItem(LoadMenuID);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
