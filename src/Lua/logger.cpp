#include "logger.hpp"

#include <list>
#include <SokuLib.h>

namespace {
    EventID renderEvent;
    std::list<std::string> logContent;
    int logFlags = 0;
}

void Logger::Initialize(int flags) {
    logFlags = flags;
    renderEvent = Soku::SubscribeEvent(SokuEvent::Render, [](void*){
        if (logContent.empty()) return;
        float width = ImGui::GetWindowWidth();
        float pos = ImGui::GetWindowPos().x;
        ImGui::PushTextWrapPos(pos + width);
        for (auto& line : logContent) {
            ImGui::Text(line.c_str());
        }
        ImGui::PopTextWrapPos();
    });
}

void Logger::Finalize() {
    Soku::UnsubscribeEvent(renderEvent);
    logContent.clear();
}

void Logger::Log(int type, const std::string& text) {
    if ((type & logFlags) == 0) return;
    logContent.push_back(text);
}
