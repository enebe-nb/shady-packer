#include "main.hpp"
#include "modpackage.hpp"
#include "asynctask.hpp"

#include <shlwapi.h>
#include <SokuLib.h>
#include <vector>
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>

namespace {
	ItemID EngineMenuID;
	EventID RenderEvtID;

	std::vector<ModPackage*> packageList;
	ModPackage* selectedPackage = 0;
	FetchJson remoteConfig("1EpxozKDE86N3Vb8b4YIwx798J_YfR_rt");

	std::string extensions[] = {".zip", "", ".dat"};
}

static ModPackage* findPackage(const std::string& name) {
	for(auto& pack : packageList) if(name == pack->name) return pack;
	return 0;
}

static bool filterPackage(const char* input, ModPackage* package)  {
	char buffer[256];
	std::transform(input, input + strlen(input), buffer, ::tolower);
	bool discard = false;
	for (char* token = strtok(buffer, " "); token; token = strtok(0, " ")) {
		bool found = false;
		if (package->nameLower.find(token) != std::string::npos) found = true;
		else for (auto& tag : package->tags) {
			if (tag.find(token) != std::string::npos) {
				found = true; break;
			}
		}
		if (!found) return false;
	}
	return true;
}

void LoadSettings() {
	iniConfig.useIntercept = GetPrivateProfileInt("Options", "useIntercept",
		false, (modulePath + "\\shady-loader.ini").c_str());
	iniConfig.autoUpdate = GetPrivateProfileInt("Options", "autoUpdate",
		true, (modulePath + "\\shady-loader.ini").c_str());
	iniConfig.useLoadLock = GetPrivateProfileInt("Options", "useLoadLock",
		true, (modulePath + "\\shady-loader.ini").c_str());
}

void SaveSettings() {
	WritePrivateProfileString("Options", "useIntercept",
		iniConfig.useIntercept ? "1" : "0", (modulePath + "\\shady-loader.ini").c_str());
	WritePrivateProfileString("Options", "autoUpdate",
		iniConfig.autoUpdate ? "1" : "0", (modulePath + "\\shady-loader.ini").c_str());
	WritePrivateProfileString("Options", "useLoadLock",
		iniConfig.useLoadLock ? "1" : "0", (modulePath + "\\shady-loader.ini").c_str());

	nlohmann::json root;
	for (auto& package : packageList) {
		root[package->name] = package->data;
		if (package->fileExists) WritePrivateProfileString("Packages", package->name.c_str(),
			package->isEnabled() ? "1" : "0", (modulePath + "\\shady-loader.ini").c_str());
	}

	std::ofstream output(modulePath + "\\packages.json");
	output << root;
}

namespace ImGui {
	bool PackageSelectable(const char* label, ImU32 color) {
		float width = ImGui::GetContentRegionAvail().x;
		ImVec2 labelSize = ImGui::CalcTextSize(label, 0, false, width - 12);
		ImVec2 frameSize(width, labelSize.y + 8);
		ImVec2 pos = ImGui::GetCursorScreenPos();

		bool clicked = ImGui::InvisibleButton(label, frameSize);
		bool hovered = ImGui::IsItemHovered();
		auto drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(pos,
			ImVec2(pos.x + frameSize.x, pos.y + frameSize.y), color);
		if (hovered) drawList->AddRectFilled(pos,
			ImVec2(pos.x + frameSize.x, pos.y + frameSize.y), IM_COL32(255, 255, 255, 64));
		ImGui::SetCursorScreenPos(ImVec2(pos.x + 6, pos.y + 4));
		ImGui::PushTextWrapPos(width - 6);
		ImGui::Text(label);
		ImGui::PopTextWrapPos();
		ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + frameSize.y));

		return clicked;
	}
}

static void EngineMenuCallback() {
	if (!remoteConfig.isRunning() && !remoteConfig.isDone()) remoteConfig.start();
	ImGui::PushID("shady-loader");
	ImVec2 windowPos = ImGui::GetWindowPos();
	char filterInput[256]; filterInput[0] = '\0';
	float screenHeight = ImGui::GetContentRegionAvail().y;

	if (ImGui::BeginChild("Package List", ImVec2(140, screenHeight), true, ImGuiWindowFlags_None)) {
		ImGui::SetNextItemWidth(140);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ImGui::InputTextWithHint("", "Search", filterInput, 256);
		ImGui::PopStyleVar();
		for (auto package : packageList) {
			if (strlen(filterInput) && !filterPackage(filterInput, package)) continue;

			ImU32 color = selectedPackage == package ? IM_COL32(0, 0, 92, 255)
				: package->isEnabled() ? IM_COL32(0, 92, 0, 255)
				: package->requireUpdate ? IM_COL32(64, 64, 0, 255)
				//: package->isLocal() ? IM_COL32(0, 64, 64, 255)
				: IM_COL32(0, 0, 0, 0);
			if (ImGui::PackageSelectable(package->name.c_str(), color)) {
				selectedPackage = package;
				if (!package->previewTask) package->downloadPreview();
			}
		}

		if (!remoteConfig.isDone()) {
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x / 2);
			ImGui::Text("%c", "|/-\\"[(int)(0.009f*GetTickCount()) & 3]);
		}
	} ImGui::EndChild();
	ImGui::PopID();

	if (selectedPackage) {
		ImGui::SameLine();
		if (ImGui::BeginChild("Package Info", ImGui::GetContentRegionAvail())) {
			ImGui::Columns(3, 0, false);
			ImGui::SetColumnWidth(-1, 6);

			ImGui::NextColumn();
			ImGui::SetColumnWidth(-1, 100);
			ImGui::Text("Name: ");
			ImGui::Text("Version: ");
			ImGui::Text("Creator: ");
			ImGui::Text("Description: ");
			ImGui::NextColumn();
			ImGui::PushTextWrapPos(ImGui::GetWindowWidth());
			ImGui::Text("%s", selectedPackage->name.c_str());
			ImGui::Text("%s", selectedPackage->version().c_str());
			ImGui::Text("%s", selectedPackage->creator().c_str());
			ImGui::Text("%s", selectedPackage->description().c_str());
			ImGui::PopTextWrapPos();

			ImGui::NextColumn(); ImGui::NextColumn();
			ImGui::Text("Tags:");

			ImGui::NextColumn();
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.00f));
			float maxWidth = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvailWidth();
			bool first = true;
			for (auto& tag : selectedPackage->tags) {
				float tagWidth = ImGui::CalcTextSize(tag.c_str()).x + 12;
				if (!first && ImGui::GetItemRectMax().x + tagWidth < maxWidth) ImGui::SameLine();
				ImGui::SmallButton(tag.c_str());
				first = false;
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(3);

			ImGui::NextColumn(); ImGui::NextColumn();
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
			ImGui::Text("Local Package:"); ImGui::SameLine();
			ImGui::NextColumn();
			if (selectedPackage->isLocal()) ImGui::TextColored(ImVec4(.4f, .4f, 1.f, 1.f), "true");
			else ImGui::TextColored(ImVec4(.7f, .7f, .7f, 1.f), "false");

			if (selectedPackage->fileExists) {
				if (selectedPackage->requireUpdate) {
					ImGui::NextColumn(); ImGui::NextColumn();
					ImGui::Text("Status:"); ImGui::NextColumn();
					if (selectedPackage->downloadTask) {
						ImGui::TextColored(ImVec4(.7f, .7f, .2f, 1.f), "Downloading...");
					} else {
						ImGui::TextColored(ImVec4(.7f, .7f, .2f, 1.f), "Found Updates");
						if (ImGui::Button("Download")) selectedPackage->downloadFile();
					}
				}

				ImGui::NextColumn(); ImGui::NextColumn();
				ImGui::Text("Enabled:"); ImGui::NextColumn();
				if (selectedPackage->isEnabled())
					ImGui::TextColored(ImVec4(.4f, 1.f, .4f, 1.f), "true");
				else ImGui::TextColored(ImVec4(.7f, .7f, .7f, 1.f), "false");

				if (ImGui::Button(selectedPackage->isEnabled() ? "Disable" : "Enable")) {
					selectedPackage->setEnabled(!selectedPackage->isEnabled());
					SaveSettings();
				}
			} else {
				ImGui::NextColumn(); ImGui::NextColumn();
				ImGui::Text("Status: "); ImGui::NextColumn();
				if (selectedPackage->downloadTask) {
					ImGui::TextColored(ImVec4(.7f, .7f, .2f, 1.f), "Downloading...");
				} else {
					ImGui::TextColored(ImVec4(.8f, .3f, .3f, 1.f), "Not Downloaded");
					if (ImGui::Button("Download")) selectedPackage->downloadFile();
				}
			}
			ImGui::PopStyleVar(2);

			ImGui::Columns();
			if (selectedPackage->previewTask) {
				if (!selectedPackage->previewTask->isDone()) {
					ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2,
						screenHeight - ImGui::GetTextLineHeightWithSpacing()));
					ImGui::Text("%c", "|/-\\"[(int)(0.009f*GetTickCount()) & 3]);
				} else if (selectedPackage->previewTask->texture) {
					ImGui::SetCursorPos(ImVec2(0, screenHeight - 270));
					ImGui::Image(selectedPackage->previewTask->texture->id, ImVec2(360, 270));
				}
			}
		} ImGui::EndChild();
	}
}

static void RenderCallback(SokuData::RenderData* data) {
	static std::string helpInfo;
	static int helpTimeout = -1;
	static bool remoteLoaded = false;
	if (!remoteLoaded && remoteConfig.isDone()) {
		remoteLoaded = true;
		// We are sync here
		if (!remoteConfig.data.is_object()) return;
		for (auto& entry : remoteConfig.data.items()) {
			ModPackage* package = findPackage(entry.key());
			if (package) {
				package->merge(entry.value());
				if (iniConfig.autoUpdate
					&& package->requireUpdate
					&& package->version().size()) {
					package->downloadFile();
					helpInfo = "Updating " + package->name;
					helpTimeout = 240;
				}
			} else {
				packageList.push_back(package = new ModPackage(entry.key(), entry.value()));
				package->data.erase("version");
			}
		}
	}

	for (auto& package : packageList) {
		if (package->downloadTask && package->downloadTask->isDone()) {
			bool wasEnabled = package->isEnabled();
			if (wasEnabled) package->setEnabled(false);
			std::string filename(modulePath + "\\" + package->name + package->ext);
			if (PathFileExists((filename + ".part").c_str())
				&& MoveFileEx((filename + ".part").c_str(), filename.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
				package->data["version"] = remoteConfig.data[package->name]["version"];
				package->requireUpdate = false;
				package->fileExists = true;
				helpInfo = "Finished " + package->name + " Download";
			} else {
				helpInfo = package->name + " Download Failed";
			} helpTimeout = 240;
			delete package->downloadTask;
			package->downloadTask = 0;
			if (wasEnabled) package->setEnabled(true);
			SaveSettings();
		}
	}

	if (helpTimeout > 0) {
		--helpTimeout;
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 size = ImGui::CalcTextSize(helpInfo.c_str(), 0, false, 640);
		size.x += style.WindowPadding.x * 2; size.y += style.WindowPadding.y * 2;
		ImGui::SetNextWindowPos(ImVec2(0, 480 - size.y));
		ImGui::SetNextWindowSize(size);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::Begin("", 0, ImGuiWindowFlags_NoDecoration);
		ImGui::Text(helpInfo.c_str());
		ImGui::End();
	}
}

static void LoadFromLocalData() {
	if (PathFileExists((modulePath + "\\packages.json").c_str())) {
		nlohmann::json localConfig;
		std::ifstream input(modulePath + "\\packages.json");
		try {input >> localConfig;} catch (...) {}

		for (auto& entry : localConfig.items()) {
			std::string filename(modulePath + "\\" + entry.key() + ".zip");
			if (PathFileExists(filename.c_str()))
				packageList.push_back(new ModPackage(entry.key(), entry.value()));
		}
	}
}

static void LoadFromFilesystem() {
	WIN32_FIND_DATA findData;
	HANDLE hFind;
	if ((hFind = FindFirstFile((modulePath + "\\*").c_str(), &findData)) != INVALID_HANDLE_VALUE) {
		do {
			char* ext = PathFindExtension(findData.cFileName);
			if (findData.cFileName[0] == '.') continue;
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
				|| strcmp(ext, ".zip") == 0
				|| strcmp(ext, ".dat") == 0) {
				std::string name(findData.cFileName, ext);
				if (!findPackage(name)) packageList.push_back(new ModPackage(findData.cFileName));
			}
		} while(FindNextFile(hFind, &findData));
		FindClose(hFind);
	}
}

void LoadEngineMenu() {
	LoadFromLocalData();
	LoadFromFilesystem();

	for (auto& package : packageList) {
		package->setEnabled(GetPrivateProfileInt("Packages",
			package->name.c_str(), false, (modulePath + "\\shady-loader.ini").c_str()));
	}

	EngineMenuID = Soku::AddItem(SokuComponent::EngineMenu, "Package Load", {EngineMenuCallback});
	RenderEvtID = Soku::SubscribeEvent(SokuEvent::Render, {RenderCallback});
	if (iniConfig.autoUpdate) remoteConfig.start();
}

void UnloadEngineMenu() {
	Soku::RemoveItem(EngineMenuID);
	Soku::UnsubscribeEvent(RenderEvtID);

	for (auto& package : packageList) {
		package->setEnabled(false);
	}
}
