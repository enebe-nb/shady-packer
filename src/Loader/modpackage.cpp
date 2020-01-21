#include "modpackage.hpp"
#include "main.hpp"
#include "../Lua/shady-lua.hpp"

#include <shlwapi.h>
#include <stack>

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

static void enablePackage(const std::string& name, const std::string& ext, std::vector<FileID>& sokuIds) {
	std::string path(name + ext);
	if (PathIsRelative(path.c_str())) path = modulePath + "\\" + path;

	if (iniConfig.useIntercept) {
		sokuIds.push_back(Soku::AddFile(s2ws(path)));
	} else if (ext == ".zip") {
		sokuIds.push_back(Soku::AddFile(s2ws(path)));
	} else if (ext == ".dat") {
		char magicWord[6];
		std::ifstream input; input.open(path, std::ios::binary);
		input.unsetf(std::ios::skipws);
		input.read(magicWord, 6);
		input.close();
		if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
			sokuIds.push_back(Soku::AddFile(s2ws(path)));
		} else {
			reinterpret_cast<void(*)(const char *)>(0x0040D1D0u)(path.c_str());
		}
	} else {
		unsigned int attr = GetFileAttributes(path.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES) return;
		if (attr & FILE_ATTRIBUTE_DIRECTORY) {
			std::stack<std::string> dirStack;
			dirStack.push(path);

			while (!dirStack.empty()) {
				WIN32_FIND_DATA findData;
				HANDLE hFind;
				std::string current = dirStack.top(); dirStack.pop();
				if ((hFind = FindFirstFile((current + "\\*").c_str(), &findData)) != INVALID_HANDLE_VALUE) {
					do {
						if (findData.cFileName[0] == '.') continue;
						if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
							dirStack.push(current + "\\" + findData.cFileName);
						else sokuIds.push_back(Soku::AddFile(s2ws(current + "\\" + findData.cFileName)));
					} while(FindNextFile(hFind, &findData));
					FindClose(hFind);
				}
			}
		}
	}
}

void ModPackage::setEnabled(bool value) {
	if (enabled == value) return;
	enabled = value;
	if (enabled) {
		enablePackage(name, ext, sokuIds);
		if (ShadyLua::isModuleLoaded()) {
			std::string luaFile = nameLower + ".lua";
			void* script = ShadyLua::LoadFromSoku(luaFile.c_str());
			if (script) ShadyLua::RunScript(script);
		}
	} else {
		for(auto& id : sokuIds) {
			Soku::RemoveFile(id);
		} sokuIds.clear();
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
