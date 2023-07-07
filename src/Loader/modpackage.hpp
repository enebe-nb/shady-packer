#pragma once

#include "../Core/package.hpp"
#include "../Core/util/filewatcher.hpp"
#include "asynctask.hpp"
#include <memory>

class ModPackage {
public:
    static std::filesystem::path basePath;
    static std::unique_ptr<ShadyCore::PackageEx> basePackage;
    static std::vector<ModPackage*> descPackage;
    static std::shared_mutex descMutex;
    static void LoadFromLocalData();
    static void LoadFromFilesystem();
    static void LoadFromRemote();
    static bool Notify();
    static void CheckUpdates();

    ShadyCore::Package* package = 0;
    const std::string name;
    std::filesystem::path path;
    std::string previewName;
    nlohmann::json::value_type data;
    std::vector<std::string> tags;
    bool requireUpdate = false;
    bool fileExists = false;
    bool downloading = false;
    bool downloadingPreview = false;
    ShadyUtil::FileWatcher* watcher = 0;

    inline bool isEnabled() {return package;}
    inline bool isLocal() {return fileExists && (!data.count("version") || data.value("version", "").empty());}
    inline bool hasFile() {return !data.value("file", "").empty();}
    inline std::string driveId() {return data.value("id", "");}
    inline std::string file() {return data.value("file", "");}
    inline std::string version() {return data.value("version", "");}
    inline std::string creator() {return data.value("creator", "");}
    inline std::string description() {return data.value("description", "");}
    inline std::string preview() {return data.value("preview", "");}

    void* script = 0;

    ModPackage(const std::string& name, const nlohmann::json::value_type& data);
    ModPackage(const std::filesystem::path& filename);
    ModPackage(const ModPackage&) = delete;
    ModPackage(ModPackage&&) = delete;

    void downloadFile();
    void downloadPreview();
    void merge(const nlohmann::json::value_type& remote);
};

void EnablePackage(ModPackage* package);
void DisablePackage(ModPackage* package);
