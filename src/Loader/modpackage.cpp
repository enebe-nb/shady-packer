#include "modpackage.hpp"
#include "main.hpp"
#include <shlwapi.h>

ModPackage::ModPackage(const std::string& name, const nlohmann::json::value_type& data)
    : name(name), data(data), ext(".zip") {
    nameLower.resize(name.size());
    std::transform(name.begin(), name.end(), nameLower.begin(), std::tolower);

    if (data.count("tags") && data["tags"].is_array()) for (auto& tag : data["tags"]) {
		tags.push_back(tag.get<std::string>());
	}

	std::string filename(modulePath + "\\" + name + ext);
	fileExists = PathFileExists(filename.c_str());
}

ModPackage::ModPackage(const std::string& filename) 
    : name(filename, 0, filename.find_last_of(".")), data({}) {
    nameLower.resize(name.size());
    std::transform(name.begin(), name.end(), nameLower.begin(), std::tolower);

    size_t index = filename.find_last_of(".");
    ext = index == std::string::npos ? "" : std::string(filename, index);
	fileExists = PathFileExists((modulePath + "\\" + filename).c_str());
}

ModPackage::~ModPackage() {
	if (previewTask) delete previewTask;
	if (downloadTask) delete downloadTask;
}

void ModPackage::setEnabled(bool value) {
	if (enabled == value) return;
	enabled = value;
	if (enabled) {
		script = EnablePackage(name, ext, sokuIds);
	} else {
		DisablePackage(sokuIds, script);
	}
}

void ModPackage::downloadFile() {
    if (downloadTask) return;
    std::string filename = modulePath + "\\" + name + (ext = ".zip") + ".part";
    if (!DeleteFile(filename.c_str())
        && GetLastError() == ERROR_ACCESS_DENIED) return;

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
	data["tags"].clear();
	tags.clear();

	if (remote.count("tags") && remote["tags"].is_array()) for (auto& tag : remote["tags"]) {
		data["tags"].push_back(tag);
		tags.push_back(tag.get<std::string>());
	}

	requireUpdate = data.value("version", "") != remote.value("version", "");
}
