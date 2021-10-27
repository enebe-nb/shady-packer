#pragma once

#include <SokuLib.hpp>

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
    SokuLib::MenuCursor modCursor;
    SokuLib::Sprite viewTitle;
    SokuLib::Sprite viewContent;
    SokuLib::Sprite viewOption;
    SokuLib::Sprite viewPreview;
    SokuLib::MenuCursor viewCursor;
    int scrollPos = 0;
    int state = 0;
    int optionCount = 0;
    int options[2];

public:
    ModMenu();
    ~ModMenu() override;
	void _() override;
	int onProcess() override;
	int onRender() override;
    void updateView(int index);
};
