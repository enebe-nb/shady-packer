#include "modpackage.hpp"
#include "main.hpp"
#include "../Core/package.hpp"
#include "th123intl.hpp"
#include <fstream>
#include <mutex>
#include <cctype>
#include <zip.h>

std::filesystem::path ModPackage::basePath(std::filesystem::current_path());
std::unique_ptr<ShadyCore::PackageEx> ModPackage::basePackage;
std::vector<ModPackage*> ModPackage::descPackage;
std::shared_mutex ModPackage::descMutex;

static ModPackage* findPackage(const std::string& name) {
	for(auto& pack : ModPackage::descPackage) if(name == pack->name) return pack;
	return 0;
}

static bool extractZipToFolder(const std::filesystem::path& filename, const std::filesystem::path& target, size_t& offset) {
	auto zsrc = zip_source_win32w_create(filename.c_str(), 0, ZIP_LENGTH_TO_END, 0);
	if (!zsrc) return false;
	auto zobj = zip_open_from_source(zsrc, ZIP_RDONLY, 0);
	if (!zobj) { zip_source_close(zsrc); return false; }

	bool isSuccessful = true;
	for (;offset < zip_get_num_entries(zobj, 0); ++offset) {
		zip_stat_t stats; zip_stat_init(&stats);
		if (zip_stat_index(zobj, offset, ZIP_STAT_NAME, &stats) == -1) continue;
		std::wstring resname; th123intl::ConvertCodePage(CP_UTF8, stats.name, resname);
		if (resname.empty()) continue;
		if (resname.back() == '/') {
			std::filesystem::create_directory(target / resname);
		} else {
			std::ofstream output(target / resname, std::ios::binary | std::ios::trunc, _SH_DENYRW);
			if (!output.is_open()) { isSuccessful = false; break; }

			auto zfile = zip_fopen_index(zobj, offset, 0);
			if (!zfile) continue;
			char buffer[4096]; zip_int64_t read = 0;
			while((read = zip_fread(zfile, buffer, 4096)) > 0) {
				output.write(buffer, read);
			} zip_fclose(zfile);
		}
	}
	zip_close(zobj);
	zip_source_close(zsrc);
	return isSuccessful;
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
				std::unique_lock lock(ModPackage::descMutex);
				for (auto& entry : remoteConfig->data.items()) {
					ModPackage* package = findPackage(entry.key());
					if (package) {
						package->merge(entry.value());
					} else {
						ModPackage::descPackage.push_back(package = new ModPackage(entry.key(), entry.value()));
						package->data["remoteVersion"] = package->data.value("version", "");
						package->data.erase("version");
					}
				}
			}

			if (iniAutoUpdate) {
				std::shared_lock lock(ModPackage::descMutex);
				for (auto& package : ModPackage::descPackage) {
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
						ModPackage::descMutex.lock();
						std::filesystem::path filename(p->path);

						filename += L".part";
						if (std::filesystem::exists(filename)) {
							std::filesystem::path target(filename);
							target.replace_extension();
							std::error_code err;
							if (std::filesystem::is_directory(target)) {
								if (!extractZipToFolder(filename, target, i->second->resumeOffset)) {
									err = std::make_error_code(std::errc::device_or_resource_busy);
								} else std::filesystem::remove(filename);
							} else {
								std::filesystem::remove(target, err);
								if (!err ) std::filesystem::rename(filename, target, err);	
							}

							if (!err) {
								p->data["version"] = p->data.value("remoteVersion", "");
								p->requireUpdate = false;
								p->fileExists = true;
							}

							if (err) {
								ModPackage::descMutex.unlock();
								++i; continue;
							}
						}

						p->downloading = false;
						ModPackage::descMutex.unlock();
						SaveSettings();

 						delete i->second;
						i = fileTasks.erase(i);
						_notify = true;
					} else ++i;
				}

				for (auto i = imageTasks.begin(); i != imageTasks.end();) {
					if (i->second->isDone()) {
						ModPackage* p = i->first;
						std::unique_lock lock(ModPackage::descMutex);

						if (!i->second->hasError) {
							p->previewName = "shady_preview_" + p->name + ".png";
							ModPackage::basePackage->insert(p->previewName, i->second->data);
							p->downloadingPreview = false;
						}

 						delete i->second;
						i = imageTasks.erase(i);
						_notify = true;
					} else ++i;
				}

				std::this_thread::sleep_for(200ms);
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
			std::filesystem::path filename(package->path);
			filename += ".part";
			try {
				std::filesystem::remove(filename);
			} catch (std::filesystem::filesystem_error err) {return;}
			FetchFile* task = new FetchFile(package->hasFile() ? package->file() : package->driveId(), filename);
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

static inline std::string ws2utf(const std::wstring_view& wstr) {
    std::string ret; th123intl::ConvertCodePage(wstr, CP_UTF8, ret);
    return ret;
}

static inline std::wstring utf2ws(const std::string_view& str) {
    std::wstring ret; th123intl::ConvertCodePage(CP_UTF8, str, ret);
    return ret;
}

ModPackage::ModPackage(const std::string& name, const nlohmann::json::value_type& data)
	: name(name), path(basePath / (utf2ws(name) + L".zip")), data(data), fileExists(std::filesystem::exists(path)) {

	if (data.count("tags") && data["tags"].is_array()) for (auto& tag : data["tags"]) {
		tags.push_back(tag.get<std::string>());
	};
}

ModPackage::ModPackage(const std::filesystem::path& filename) 
    : name(ws2utf(filename.stem().c_str())), path(filename), data({}), fileExists(std::filesystem::exists(filename)) {}

void ModPackage::downloadFile() { downloadController.downloadFile(this); }
void ModPackage::downloadPreview() { downloadController.downloadImage(this); }

void ModPackage::merge(const nlohmann::json::value_type& remote) {
	data["id"] = remote.value("id", "");
	data["file"] = remote.value("file", "");
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

		std::unique_lock lock(descMutex);
		for (auto& entry : localConfig.items()) {
			std::filesystem::path filename(basePath / (utf2ws(entry.key()) + L".zip"));
			if (std::filesystem::exists(filename))
				descPackage.push_back(new ModPackage(entry.key(), entry.value()));
		}
	}
}

void ModPackage::LoadFromFilesystem() {
	for (std::filesystem::directory_iterator iter(basePath), end; iter != end; ++iter) {
		bool isDir = std::filesystem::is_directory(iter->path());
		auto ext = iter->path().extension();
		if (isDir || ext == ".zip" || ext == ".dat") {
			if (iter->path().stem() == "shady-loader") continue;
			std::unique_lock lock(descMutex);
			if (!findPackage(ws2utf(iter->path().stem().c_str()))) descPackage.push_back(new ModPackage(iter->path()));
		}
	}
}

void ModPackage::LoadFromRemote() { downloadController.fetchRemote(); }
bool ModPackage::Notify() { return downloadController.notify(); }

void LoadPackage() {
	ModPackage::LoadFromLocalData();
	ModPackage::LoadFromFilesystem();
	const std::filesystem::path spath = ModPackage::basePath / L"shady-loader.ini";
	std::shared_lock lock(ModPackage::descMutex);

	{ wchar_t buffer[4096], *context = 0; int i = 0;
		GetPrivateProfileStringW(L"Options", L"order", L"", buffer, 4096, spath.c_str());
		for(wchar_t* token = wcstok_s(buffer, L",", &context); token; token = wcstok_s(0, L",", &context)) {
			std::string name; th123intl::ConvertCodePage(token, CP_UTF8, name);
			if (i >= ModPackage::descPackage.size()) break;
			if (ModPackage::descPackage[i]->name == name) {++i; continue;}
			for (int j = i + 1; j < ModPackage::descPackage.size(); ++j)
				if (ModPackage::descPackage[j]->name == name) {
					ModPackage* aux = ModPackage::descPackage[j];
					ModPackage::descPackage[j] = ModPackage::descPackage[i];
					ModPackage::descPackage[i] = aux;
					++i; break;
				}
		}
	}

	for (auto& package : ModPackage::descPackage) {
		std::string name = (package->name[0] == '[' ? "<" : "") + package->name;
		if (GetPrivateProfileIntW(L"Packages", utf2ws(name).c_str(), false, spath.c_str())) {
			EnablePackage(package);
			package->watcher = ShadyUtil::FileWatcher::create(package->path);
		}
	}

	if (iniAutoUpdate) ModPackage::LoadFromRemote();
	std::filesystem::path loaderPack = ModPackage::basePath / L"shady-loader.dat";
	if (std::filesystem::exists(loaderPack)
		|| std::filesystem::exists(loaderPack.replace_extension(L".zip"))
		|| std::filesystem::exists(loaderPack.replace_extension(L""))) {
		ModPackage::basePackage->merge(loaderPack);
	}
}

void UnloadPackage() {
	std::unique_lock lock(ModPackage::descMutex);
	for (auto& package : ModPackage::descPackage) {
		if (package->isEnabled()) {
			//DisablePackage(package);
			if (package->watcher) {
				delete package->watcher;
				package->watcher = 0;
			}
		}
	}

	ModPackage::descPackage.clear();
}

void ModPackage::CheckUpdates() {
	ShadyUtil::FileWatcher* current;
	while(current = ShadyUtil::FileWatcher::getNextChange()) {
		ModPackage* package = 0;
		{ std::shared_lock lock(ModPackage::descMutex);
		for (auto p : ModPackage::descPackage) {
			if (p->watcher == current) { package = p; break; }
		} }

		if (package) switch (current->action) {
		case ShadyUtil::FileWatcher::CREATED:
			EnablePackage(package);
			break;
		case ShadyUtil::FileWatcher::REMOVED:
			DisablePackage(package);
			break;
		case ShadyUtil::FileWatcher::RENAMED:
			package->path.replace_filename(current->filename);
			break;
		case ShadyUtil::FileWatcher::MODIFIED:
			DisablePackage(package);
			EnablePackage(package);
			break;
		}
	}
}