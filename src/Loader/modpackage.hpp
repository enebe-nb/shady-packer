#pragma once

#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include "asynctask.hpp"
#include <SokuLib.hpp>
#include "design.hpp"

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

    //void setEnabled(bool value);
    void downloadFile();
    void downloadPreview();
    void merge(const nlohmann::json::value_type& remote);
};

class ModList : public SokuLib::CFileList {
public:
    SokuLib::CDesign::Gauge* scrollBar;
    int scrollLen;

    ModList();
    void renderScroll(float x, float y, int offset, int size, int view);

    virtual void updateList() override;
    virtual int appendLine(SokuLib::String& out, void* unknown, SokuLib::Deque<SokuLib::String>& list, int index) override;
};

class ModMenu : public SokuLib::IMenu {
private:
    bool syncing = false;
    SokuLib::CDesign design;
    ModList modList;
    SokuLib::InputHandler modCursor;
    SokuLib::CSprite viewTitle;
    SokuLib::CSprite viewContent;
    SokuLib::CSprite viewOption;
    SokuLib::InputHandler viewCursor;
    int scrollPos = 0;
    int state = 0;
    int options = 0;

public:
    ModMenu();
    ~ModMenu() override;
	void _() override;
	int onProcess() override;
	int onRender() override;
    void updateView(int index);
};

void EnablePackage(ModPackage* package);
void DisablePackage(ModPackage* package);
