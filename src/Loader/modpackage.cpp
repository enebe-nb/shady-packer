#include "modpackage.hpp"
#include "main.hpp"
#include "../Core/package.hpp"
#include <fstream>
#include <mutex>
// NOTE: Don't use wchar is this file, so it can be build on linux

std::filesystem::path ModPackage::basePath(std::filesystem::current_path());
std::vector<ModPackage*> ModPackage::packageList;
std::shared_mutex ModPackage::packageListMutex;
extern ShadyCore::Package package;

static ModPackage* findPackage(const std::string& name) {
	for(auto& pack : ModPackage::packageList) if(name == pack->name) return pack;
	return 0;
}

namespace {
	class DownloadController {
	private:
		std::mutex _mutex;
		std::thread* _thread;
		bool _notify = false, _done = false, _sync = false;

		FetchJson* remoteConfig = 0;
		std::list<std::pair<ModPackage*, FetchFile*> > fileTasks;
		std::list<std::pair<ModPackage*, FetchImage*> > imageTasks;

		void sync() {
			using namespace std::chrono_literals;
			if (remoteConfig != 0) throw std::runtime_error("Sync has already started");

			remoteConfig = new FetchJson(iniRemoteConfig);
			remoteConfig->start();
			while(!remoteConfig->isDone()) std::this_thread::sleep_for(50ms);

			{
				std::unique_lock lock(ModPackage::packageListMutex);
				for (auto& entry : remoteConfig->data.items()) {
					ModPackage* package = findPackage(entry.key());
					if (package) {
						package->merge(entry.value());
					} else {
						ModPackage::packageList.push_back(package = new ModPackage(entry.key(), entry.value()));
						package->data["remoteVersion"] = package->data.value("version", "");
						package->data.erase("version");
					}
				}
			}

			if (iniAutoUpdate) {
				std::shared_lock lock(ModPackage::packageListMutex);
				for (auto& package : ModPackage::packageList) {
					if (!package->isLocal() && package->requireUpdate) package->downloadFile();
				}
			}

			_notify = _sync = true;
		}

		void run() {
			using namespace std::chrono_literals;
			if (!_sync) sync();

			_mutex.lock();
			while(!fileTasks.empty() || !imageTasks.empty()) {
				_mutex.unlock();

				for (auto i = fileTasks.begin(); i != fileTasks.end();) {
					if (i->second->isDone()) {
						ModPackage* p = i->first;
						ModPackage::packageListMutex.lock();
						bool wasEnabled = p->enabled;
						if (wasEnabled) {
							if (iniUseLoadLock) loadLock.lock();
							DisablePackage(p);
						}

						std::filesystem::path filename(ModPackage::basePath / p->name);
						filename += p->ext; filename += L".part";
						if (std::filesystem::exists(filename)) {
							std::filesystem::path target(filename);
							target.replace_extension();
							std::error_code err;
							std::filesystem::remove(target, err);
							if (!err) std::filesystem::rename(filename, target, err);
							if (!err) {
								p->data["version"] = p->data.value("remoteVersion", "");
								p->requireUpdate = false;
								p->fileExists = true;
							}

							// TODO test remove
							if (err) throw std::runtime_error(std::string("Failed to Rename: ") + err.message());
						}

						if (wasEnabled) {
							EnablePackage(p);
							if (iniUseLoadLock) loadLock.unlock();
						}
						p->downloading = false;
						ModPackage::packageListMutex.unlock();
						SaveSettings();

 						delete i->second;
						i = fileTasks.erase(i);
						_notify = true;
					} else ++i;
				}

				for (auto i = imageTasks.begin(); i != imageTasks.end();) {
					if (i->second->isDone()) {
						ModPackage* p = i->first;
						std::unique_lock lock(ModPackage::packageListMutex);

						if (!i->second->hasError) {
							p->previewPath = "shady_preview_" + p->name.string() + ".png";
							std::transform(p->previewPath.begin() + 14, p->previewPath.end(), p->previewPath.begin() + 14, std::tolower);
							package.appendFile(p->previewPath.c_str(), i->second->data);
						} p->downloadingPreview = false;

 						delete i->second;
						i = imageTasks.erase(i);
						_notify = true;
					} else ++i;
				}

				std::this_thread::sleep_for(50ms);
				_mutex.lock();
			}

			_done = true;
			_mutex.unlock();
		}

		void tryJoin() {
			if (!_done || !_thread) return;

			_thread->join();
			delete _thread;
			_thread = 0;
		}

		void tryStart() {
			tryJoin();
			if (!_thread) {
				_done = false;
				_thread = new std::thread(&DownloadController::run, this);
			}
		}

	public:
		bool notify() {
			std::lock_guard lock(_mutex);
			tryJoin();

			if (_notify) {
				_notify = false;
				return true;
			} return false;
		}

		void downloadFile(ModPackage* package) {
			std::lock_guard lock(_mutex);
			if (package->downloading) return;

			package->downloading = true;
			std::filesystem::path filename(ModPackage::basePath / package->name);
			filename += (package->ext = ".zip"); filename += ".part";
			try {
				std::filesystem::remove(filename);
			} catch (std::filesystem::filesystem_error err) {return;}
			FetchFile* task = new FetchFile(package->driveId(), filename);
			fileTasks.emplace_back(package, task);
			task->start();

			tryStart();
		}

		void downloadImage(ModPackage* package) {
			std::lock_guard lock(_mutex);
			if (package->downloadingPreview || package->preview().empty()) return;

			package->downloadingPreview = true;
			FetchImage* task = new FetchImage(package->preview());
			imageTasks.emplace_back(package, task);
			task->start();

			tryStart();
		}

		void fetchRemote() {
			std::lock_guard lock(_mutex);

			tryStart();
		}
	} downloadController;
}

ModPackage::ModPackage(const std::string& name, const nlohmann::json::value_type& data)
    : name(name), ext(".zip"), data(data) {

    if (data.count("tags") && data["tags"].is_array()) for (auto& tag : data["tags"]) {
		tags.push_back(tag.get<std::string>());
	}

	std::filesystem::path path(basePath / name); path += ext;
	fileExists = std::filesystem::exists(path);
}

ModPackage::ModPackage(const std::filesystem::path& filename) 
    : name(filename.stem()), ext(filename.extension()), data({}) {

	fileExists = std::filesystem::exists(basePath / filename);
}

void ModPackage::downloadFile() { downloadController.downloadFile(this); }
void ModPackage::downloadPreview() { downloadController.downloadImage(this); }

void ModPackage::merge(const nlohmann::json::value_type& remote) {
	data["id"] = remote.value("id", "");
	data["creator"] = remote.value("creator", "");
	data["description"] = remote.value("description", "");
	data["preview"] = remote.value("preview", "");
    data["remoteVersion"] = remote.value("version", "");
	data["tags"].clear();
	tags.clear();

	if (remote.count("tags") && remote["tags"].is_array()) for (auto& tag : remote["tags"]) {
		data["tags"].push_back(tag);
		tags.push_back(tag.get<std::string>());
	}

	requireUpdate = data.value("version", "") != remote.value("version", "");
}

void ModPackage::LoadFromLocalData() {
	if (std::filesystem::exists(basePath / "packages.json")) {
		nlohmann::json localConfig;
		std::ifstream input(basePath / "packages.json");
		try {input >> localConfig;} catch (...) {}

		std::shared_lock lock(packageListMutex);
		for (auto& entry : localConfig.items()) {
			std::filesystem::path filename(basePath / (entry.key() + ".zip"));
			if (std::filesystem::exists(filename))
				packageList.push_back(new ModPackage(entry.key(), entry.value()));
		}
	}
}

void ModPackage::LoadFromFilesystem() {
	for (std::filesystem::directory_iterator iter(basePath), end; iter != end; ++iter) {
        bool isDir = std::filesystem::is_directory(iter->path());
        auto ext = iter->path().extension();
        if (isDir || ext == ".zip" || ext == ".dat") {
			if (iter->path().stem() == "shady-loader") continue;
			std::shared_lock lock(packageListMutex);
			if (!findPackage(iter->path().stem().string())) packageList.push_back(new ModPackage(iter->path().filename()));
		}
	}
}

void ModPackage::LoadFromRemote() { downloadController.fetchRemote(); }
bool ModPackage::Notify() { return downloadController.notify(); }
