#include "modpackage.hpp"
#include <fstream>
// NOTE: Don't use wchar is this file, so it can be build on linux

std::filesystem::path ModPackage::basePath(std::filesystem::current_path());
std::vector<ModPackage*> ModPackage::packageList;

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

ModPackage::~ModPackage() {
	if (previewTask) delete previewTask;
	if (downloadTask) delete downloadTask;
}

void ModPackage::downloadFile() {
    if (downloadTask) return;
    std::filesystem::path filename(basePath / name);
	filename += (ext = ".zip"); filename += ".part";
	try {
		std::filesystem::remove(filename);
	} catch (std::filesystem::filesystem_error err) {return;}
	
    downloadTask = new FetchFile(driveId(), filename);
    downloadTask->start();
}

void ModPackage::downloadPreview() {
    if (previewTask) return;
	if (preview().empty()) return;
    previewTask = new FetchImage(preview());
    previewTask->start();
}

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

static ModPackage* findPackage(const std::string& name) {
	for(auto& pack : ModPackage::packageList) if(name == pack->name) return pack;
	return 0;
}

void ModPackage::LoadFromLocalData() {
	if (std::filesystem::exists(basePath / "packages.json")) {
		nlohmann::json localConfig;
		std::ifstream input(basePath / "packages.json");
		try {input >> localConfig;} catch (...) {}

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
			if (!findPackage(iter->path().stem().string())) packageList.push_back(new ModPackage(iter->path().filename()));
		}
	}
}

void ModPackage::LoadFromRemote(const nlohmann::json& data) {
    for (auto& entry : data.items()) {
        ModPackage* package = findPackage(entry.key());
        if (package) {
            package->merge(entry.value());
        } else {
            packageList.push_back(package = new ModPackage(entry.key(), entry.value()));
            package->data["remoteVersion"] = package->data.value("version", "");
            package->data.erase("version");
        }
    }
}