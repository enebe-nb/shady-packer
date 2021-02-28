#include "main.hpp"
#include "modpackage.hpp"
#include "asynctask.hpp"

#include <SokuLib.h>
#include <vector>
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>

namespace {
	ItemID EngineMenuID;
	EventID RenderEvtID;

	ModPackage* selectedPackage = 0;
	FetchJson remoteConfig("1EpxozKDE86N3Vb8b4YIwx798J_YfR_rt");
}

static void setPackageEnabled(ModPackage* package, bool value) {
	if (package->enabled == value) return;
	package->enabled = value;
	if (package->enabled) {
		package->packageId = EnablePackage(package->name, package->ext);
	} else {
		DisablePackage(package->packageId, package->script);
	}
}

/*
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
		//ImGui::SetNextItemWidth(140);
		//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		//ImGui::InputTextWithHint("", "Search", filterInput, 256);
		//ImGui::PopStyleVar();
		for (auto package : ModPackage::packageList) {
			//if (strlen(filterInput) && !filterPackage(filterInput, package)) continue;

			ImU32 color = selectedPackage == package ? IM_COL32(0, 0, 92, 255)
				: package->enabled ? IM_COL32(0, 92, 0, 255)
				: package->requireUpdate ? IM_COL32(64, 64, 0, 255)
				//: package->isLocal() ? IM_COL32(0, 64, 64, 255)
				: IM_COL32(0, 0, 0, 0);
			if (ImGui::PackageSelectable(package->name.string().c_str(), color)) {
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
				if (selectedPackage->enabled)
					ImGui::TextColored(ImVec4(.4f, 1.f, .4f, 1.f), "true");
				else ImGui::TextColored(ImVec4(.7f, .7f, .7f, 1.f), "false");

				if (ImGui::Button(selectedPackage->enabled ? "Disable" : "Enable")) {
					setPackageEnabled(selectedPackage, !selectedPackage->enabled);
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
				//} else if (selectedPackage->previewTask->texture) {
					//ImGui::SetCursorPos(ImVec2(0, screenHeight - 270));
					//ImGui::Image(selectedPackage->previewTask->texture->id, ImVec2(360, 270));
				}
			}
		} ImGui::EndChild();
	}
}
*/

namespace{
	class : public AsyncTask {
	protected:
		void run() {
			using namespace std::chrono_literals;
			std::list<ModPackage*> downloads;

			// Wait for module database
			while (true) {
				//static std::string helpInfo;
				//static int helpTimeout = -1;
				if (remoteConfig.isDone()) {
					// We are sync here
					if (!remoteConfig.data.is_object()) return;
					ModPackage::LoadFromRemote(remoteConfig.data);
					if (iniAutoUpdate) for (auto& package : ModPackage::packageList) {
						if (!package->isLocal() && package->requireUpdate) {
							package->downloadFile();
							downloads.push_back(package);
							//helpInfo = "Updating " + package->name.string();
							//helpTimeout = 240;
						}
					}
					break;
				}
				std::this_thread::sleep_for(100ms);
			}

			while (!downloads.empty()) {
				for (auto i = downloads.begin(); i != downloads.end();) {
					if ((*i)->downloadTask && (*i)->downloadTask->isDone()) {
						ModPackage* package = (*i);
						bool wasEnabled = package->enabled;
						if (wasEnabled) {
							if (iniUseLoadLock) loadLock.lock();
							setPackageEnabled(package, false);
						}
						std::filesystem::path filename(ModPackage::basePath / package->name);
						filename += package->ext; filename += L".part";
						if (std::filesystem::exists(filename)) {
							std::filesystem::path target(filename);
							target.replace_extension();
							std::filesystem::rename(filename, target);
							package->data["version"] = package->data.value("remoteVersion", "");
							package->requireUpdate = false;
							package->fileExists = true;
							//helpInfo = "Finished " + package->name.string() + " Download";
						} else {
							//helpInfo = package->name.string() + " Download Failed";
						} //helpTimeout = 240;
						delete package->downloadTask;
						package->downloadTask = 0;
						if (wasEnabled) {
							setPackageEnabled(package, true);
							if (iniUseLoadLock) loadLock.lock();
						}
						SaveSettings();
						i = downloads.erase(i);
					} else ++i;
				}
				std::this_thread::sleep_for(100ms);
			}
		}
	} updateController;
}

/*
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
*/

void LoadEngineMenu() {
	ModPackage::LoadFromLocalData();
	ModPackage::LoadFromFilesystem();

	for (auto& package : ModPackage::packageList) {
		bool isEnabled = GetPrivateProfileIntW(L"Packages", package->name.c_str(),
			false, (ModPackage::basePath / L"shady-loader.ini").c_str());
		setPackageEnabled(package, isEnabled);
	}

	if (hasSokuEngine) {
		//EngineMenuID = Soku::AddItem(SokuComponent::EngineMenu, "Package Load", {EngineMenuCallback});
		//RenderEvtID = Soku::SubscribeEvent(SokuEvent::Render, {RenderCallback});
	}
	if (iniAutoUpdate) {
		remoteConfig.start();
		updateController.start();
	}
}

void UnloadEngineMenu() {
	if (hasSokuEngine) {
		//Soku::RemoveItem(EngineMenuID);
		//Soku::UnsubscribeEvent(RenderEvtID);
	}

	loadLock.lock();
	for (auto& package : ModPackage::packageList) {
		setPackageEnabled(package, false);
		delete package;
	}
	loadLock.lock();
	ModPackage::packageList.clear();
}
