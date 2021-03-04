#include "main.hpp"
#include "modpackage.hpp"
#include "asynctask.hpp"

#include <vector>
#include <list>
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>

namespace {
	ModPackage* selectedPackage = 0;
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
			std::list<ModPackage*> downloads;

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
						bool success = false;
						if (std::filesystem::exists(filename)) {
							std::filesystem::path target(filename);
							target.replace_extension();
							std::error_code err;
							std::filesystem::remove(target, err);
							if (!err) std::filesystem::rename(filename, target, err);
							if (!err) {
								success = true;
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
				std::this_thread::sleep_for(100ms);
			}
		}
	} updateController;
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
