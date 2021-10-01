#pragma once

#include "asynctask.hpp"

class ModPackage {
public:
    static std::filesystem::path basePath;
    static std::vector<ModPackage*> packageList;
    static void LoadFromLocalData();
    static void LoadFromFilesystem();
    static void LoadFromRemote(const nlohmann::json& data);

    bool enabled = false;
    std::filesystem::path name;
    std::filesystem::path ext;
    nlohmann::json data;
    int packageId;
    std::vector<std::string> tags;
    bool requireUpdate = false;
    bool fileExists = false;

    inline bool isLocal() {return fileExists && (!data.count("version") || data.value("version", "").empty());}
    inline std::string driveId() {return data.value("id", "");}
    inline std::string version() {return data.value("version", "");}
    inline std::string creator() {return data.value("creator", "");}
    inline std::string description() {return data.value("description", "");}
    inline std::string preview() {return data.value("preview", "");}

    FetchImage* previewTask = 0;
    FetchFile* downloadTask = 0;
    void* script = 0;

    ModPackage(const std::string& name, const nlohmann::json::value_type& data);
    ModPackage(const std::filesystem::path& filename);
    ModPackage(const ModPackage&) = delete;
    ModPackage(ModPackage&&) = delete;
    ~ModPackage();

    void downloadFile();
    void downloadPreview();
    void merge(const nlohmann::json::value_type& remote);
};

void EnablePackage(ModPackage* package);
void DisablePackage(ModPackage* package);
