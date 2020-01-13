#pragma once

#include <string>
#include <thread>
#include <vector>
#include <SokuLib.h>
#include <nlohmann/json.hpp>
#include "asynctask.hpp"

class ModPackage {
private:
    bool enabled = false;
public:
    std::string name;
    std::string nameLower;
    std::string ext;
    nlohmann::json data;
    std::vector<FileID> sokuIds;
    std::vector<std::string> tags;
    bool requireUpdate = false;
    bool fileExists = false;

    inline bool isEnabled() {return enabled;}
    inline bool isLocal() {return fileExists && (!data.count("version") || data.value("version", "").empty());}
    inline std::string driveId() {return data.value("id", "");}
    inline std::string version() {return data.value("version", "");}
    inline std::string creator() {return data.value("creator", "");}
    inline std::string description() {return data.value("description", "");}
    inline std::string preview() {return data.value("preview", "");}

    FetchImage* previewTask = 0;
    FetchFile* downloadTask = 0;

    ModPackage(const std::string& name, const nlohmann::json::value_type& data);
    ModPackage(const std::string& filename);
    ModPackage(const ModPackage&) = delete;
    ModPackage(ModPackage&&) = delete;
    ~ModPackage();

    void setEnabled(bool value);
    void downloadFile();
    void downloadPreview();
    void merge(const nlohmann::json::value_type& remote);
    /*
    bool DrawMenuItem(bool selected, float width = -1);
    enum Command {CMD_ENABLE, CMD_DOWNLOAD, CMD_LINK};
    Command DrawInfo();
    */
};

//inline bool operator==(const ModPackage& l, const ModPackage& r)
    //{ return l.name == r.name; }
//inline bool operator<(const ModPackage& l, const ModPackage& r)
    //{ return l.name < r.name; }