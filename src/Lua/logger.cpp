#include "logger.hpp"

#include <fstream>
//#include <ctime>
#include <list>
#include <mutex>
#include <SokuLib.hpp>

namespace {
    SokuLib::SWRFont* consoleFont = 0;
    SokuLib::CSprite consoleView;
    std::mutex consoleLock;
    int logFlags, logTimeout = 0;
    bool isDirty = false;

    std::list<std::string> logContent;
}

static SokuLib::SWRFont* createFont() {
    SokuLib::SWRFont* font = new SokuLib::SWRFont;
    font->create();
    font->setIndirect(SokuLib::FontDescription {
        "Courier New",                      // faceName
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // color
        14, 500,                            // height, weight
        false, true, false,                 // italic, shadow, wrap
        100000,                             // bufferSize ???
        0, 0, 0, 0,                         // padding, spacing
    });
    return font;
}

void Logger::Initialize(int flags) {
    logFlags = flags;
}

void Logger::Finalize() {
    std::lock_guard guard(consoleLock);
    if (consoleView.texture) SokuLib::textureMgr.remove(consoleView.texture);
    logContent.clear();

    if (consoleFont) {
        consoleFont->destruct();
        delete consoleFont;
        consoleFont = 0;
    }
}

void Logger::Clear() {
    std::lock_guard guard(consoleLock);
    logContent.clear();
    isDirty = true;
}

void Logger::Render() {
    if (logContent.empty()) return;

    std::lock_guard guard(consoleLock);
    if (isDirty) {
        if (!consoleFont) consoleFont = createFont();
        isDirty = false;
        std::string content;
        for (auto& line : logContent) {
            content += line + "<br>";
        }

        if (consoleView.texture) SokuLib::textureMgr.remove(consoleView.texture);
        int texture, width, height;
        int* x = SokuLib::textureMgr.createTextTexture(&texture, content.c_str(), *consoleFont, 632, 472, &width, &height);
        consoleView.setTexture2(texture, 0, 0, width, height);
    }

    if (logTimeout > 0) {
        consoleView.render(4, 476 - consoleView.sizeY);
        if (--logTimeout == 0) logContent.clear();
    }
}

static std::string getTypeName(int type) {
    switch (type) {
        case Logger::LOG_DEBUG: return "D";
        case Logger::LOG_ERROR: return "E";
        case Logger::LOG_INFO: return "I";
        case Logger::LOG_WARNING: return "W";
        default: return "<untyped>";
    }
}

void Logger::Log(int type, const std::string& text) {
    if ((type & logFlags) == 0) return;

    std::lock_guard guard(consoleLock);
    logContent.push_back(getTypeName(type) + ": " + text);
    isDirty = true; logTimeout = 300; // TODO use time instead

    // size limiting
    while(logContent.size() > 10) logContent.pop_front();
}
