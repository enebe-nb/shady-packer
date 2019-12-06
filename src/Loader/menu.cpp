#include "main.hpp"

#include <shlwapi.h>
#include <SokuLib.h>
#include <vector>
#include <map>
#include <algorithm>

namespace {
	std::map<std::string, std::vector<ItemID> > packageList;
	std::vector<std::string> optionList;
	ItemID EngineMenuID;
}

std::wstring s2ws(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void addPackage(const std::string& name) {
	const std::string path = modulePath + "\\" + name;
	unsigned int attr = GetFileAttributes(path.c_str());
	packageList[name].push_back(Soku::AddFile(s2ws(path)));
}

void LoadSettings() {
	std::ifstream config(modulePath + "\\shady-loader.cfg");
	if (!config.fail()) {
		std::string line;
		while(std::getline(config, line)) if (line.size() && line[0] != '#') {
			addPackage(line);
			optionList.push_back(line);
		}
	}

	WIN32_FIND_DATA findData;
	HANDLE hFind;
	if ((hFind = FindFirstFile((modulePath + "\\*").c_str(), &findData)) != INVALID_HANDLE_VALUE) {
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
	std::ofstream config(modulePath + "\\shady-loader.cfg");
	for(auto& pair: packageList) {
		config << pair.first << std::endl;
	}
}

void EngineMenuCallback() {
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
				addPackage(pack);
			} else {
				for(auto& id : packageList[pack]) Soku::RemoveFile(id);
				packageList.erase(pack);
			}

			SaveSettings();
		}
	}
	ImGui::PopStyleVar();
}

void LoadEngineMenu() {
	EngineMenuID = Soku::AddItem(SokuComponent::EngineMenu, "Package Load", {EngineMenuCallback});
}

void UnloadEngineMenu() {
	Soku::RemoveItem(EngineMenuID);
}
