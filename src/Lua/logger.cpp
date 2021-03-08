#include "logger.hpp"

#include <fstream>
#include <ctime>
#include <imgui.h>
#include <list>
#include <mutex>

namespace {
    std::mutex contentLock;
    std::list<std::string> logContent;
    int logFlags;
}

void Logger::Initialize(int flags) {
    logFlags = flags;
}

void Logger::Finalize() {
    std::lock_guard guard(contentLock);
    logContent.clear();
}

void Logger::Clear() {
    std::lock_guard guard(contentLock);
    logContent.clear();
}

void Logger::Render() {
    if (logContent.empty()) return;
    float width = ImGui::GetContentRegionAvailWidth();
    ImGui::PushTextWrapPos(width - 8);

    std::lock_guard guard(contentLock);
    for (auto& line : logContent) {
        ImGui::Text(line.c_str());
    }

    ImGui::PopTextWrapPos();
}

static const char* getTypeName(int type) {
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
    logContent.push_back(std::string(getTypeName(type)) + ": " + text);
}
