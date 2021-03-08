#include "main.hpp"
#include "modpackage.hpp"
#include "asynctask.hpp"

#include <vector>
#include <list>
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>
#include <imgui.h>

namespace {
	ModPackage* selectedPackage = 0;
	std::list<ModPackage*> downloads;
	FetchJson remoteConfig("1EpxozKDE86N3Vb8b4YIwx798J_YfR_rt");
}

static void setPackageEnabled(ModPackage* package, bool value) {
	if (package->enabled == value) return;
	package->enabled = value;
	if (package->enabled) {
		EnablePackage(package);
	} else {
		DisablePackage(package);
	}
}

namespace{
	class : public AsyncTask {
	protected:
		void run() {
			using namespace std::chrono_literals;
			// Wait for module database
			while (true) {
				if (remoteConfig.isDone()) {
					// We are sync here
					if (!remoteConfig.data.is_object()) return;
					ModPackage::LoadFromRemote(remoteConfig.data);
					if (iniAutoUpdate) for (auto& package : ModPackage::packageList) {
						if (!package->isLocal() && package->requireUpdate) {
							package->downloadFile();
							downloads.push_back(package);
						}
					}
					break;
				}
				std::this_thread::sleep_for(100ms);
			}

			while (true) {
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
							std::error_code err;
							std::filesystem::remove(target, err);
							if (!err) std::filesystem::rename(filename, target, err);
							if (!err) {
								package->data["version"] = package->data.value("remoteVersion", "");
								package->requireUpdate = false;
								package->fileExists = true;
							}
						}
 						delete package->downloadTask;
						package->downloadTask = 0;
						if (wasEnabled) {
							setPackageEnabled(package, true);
							if (iniUseLoadLock) loadLock.unlock();
						}
						SaveSettings();
						i = downloads.erase(i);
					} else ++i;
				}
				std::this_thread::sleep_for(1000ms);
			}
		}
	} updateController;
}

namespace {
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

void RenderGui() {
    static bool tLoader = false;
    if ((*(int *)0x008A0044) != 2) return;

    if (tLoader) { if (ImGui::Begin("shady-loader", &tLoader)) {
        ImVec2 windowPos = ImGui::GetWindowPos();
        float height = ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginChild("Package List", ImVec2(140, height), true)) {
            for (auto package : ModPackage::packageList) {
                ImU32 color = selectedPackage == package ? IM_COL32(0, 0, 92, 255)
                    : package->enabled ? IM_COL32(0, 92, 0, 255)
                    : package->requireUpdate ? IM_COL32(64, 64, 0, 255)
                    : IM_COL32(0, 0, 0, 0);
                if (PackageSelectable(package->name.string().c_str(), color)) {
                    selectedPackage = package;
                    //if (!package->previewTask) package->downloadPreview();
                }
            }
            /*
            if (!remoteConfig.isDone()) {
                ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x / 2);
                ImGui::Text("%c", "|/-\\"[(int)(0.009f*GetTickCount()) & 3]);
            }
            */
        } ImGui::EndChild();
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
                ImGui::Text("%s", selectedPackage->name.string().c_str());
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
                if (selectedPackage->fileExists) {
                    if (selectedPackage->requireUpdate) {
                        ImGui::NextColumn(); ImGui::NextColumn();
                        ImGui::Text("Status:"); ImGui::NextColumn();
                        if (selectedPackage->downloadTask) {
                            ImGui::TextColored(ImVec4(.7f, .7f, .2f, 1.f), "Downloading...");
                        } else {
                            ImGui::TextColored(ImVec4(.7f, .7f, .2f, 1.f), "Found Updates");
                            if (ImGui::Button("Download")) {
								selectedPackage->downloadFile();
								downloads.push_back(selectedPackage);
							}
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
						if (ImGui::Button("Download")) {
							selectedPackage->downloadFile();
							downloads.push_back(selectedPackage);
						}
                    }
                }
                /*
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
                */
            } ImGui::EndChild();
        }        
    } ImGui::End(); }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0, 0);
    ImVec2 size = ImGui::CalcTextSize("shady-loader");
    ImGui::SetNextWindowPos(ImVec2(640 - size.x - 8, 480 - size.y - 8));
    ImGui::SetNextWindowSize(ImVec2(size.x + 8, size.y + 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("", 0, ImGuiWindowFlags_NoDecoration);
    if (ImGui::Button("shady-loader")) tLoader = true;
    ImGui::End();
}

void LoadPackage() {
	ModPackage::LoadFromLocalData();
	ModPackage::LoadFromFilesystem();

	for (auto& package : ModPackage::packageList) {
		bool isEnabled = GetPrivateProfileIntW(L"Packages", package->name.c_str(),
			false, (ModPackage::basePath / L"shady-loader.ini").c_str());
		setPackageEnabled(package, isEnabled);
	}

	if (iniAutoUpdate) {
		remoteConfig.start();
		updateController.start();
	}
}

void UnloadPackage() {
	loadLock.lock();
	for (auto& package : ModPackage::packageList) {
		setPackageEnabled(package, false);
		delete package;
	}
	loadLock.lock();
	ModPackage::packageList.clear();
}
