#include "logger.hpp"

#include <fstream>
#include <ctime>

namespace {
    //EventID renderEvent;
    //std::list<std::string> logContent;
    std::ofstream* outlog = 0;
    int logFlags = Logger::LOG_ERROR;
}

void Logger::Initialize(int flags) {
    logFlags = flags;
    // renderEvent = Soku::SubscribeEvent(SokuEvent::Render, [](void*){
    //     if (logContent.empty()) return;
    //     float width = ImGui::GetWindowWidth();
    //     float pos = ImGui::GetWindowPos().x;
    //     ImGui::PushTextWrapPos(pos + width);
    //     for (auto& line : logContent) {
    //         ImGui::Text(line.c_str());
    //     }
    //     ImGui::PopTextWrapPos();
    // });
}

void Logger::Finalize() {
    // Soku::UnsubscribeEvent(renderEvent);
    // logContent.clear();
    delete outlog;
}

static const char* getTypeName(int type) {
    switch (type) {
        case Logger::LOG_DEBUG: return "Debug";
        case Logger::LOG_ERROR: return "Error";
        case Logger::LOG_INFO: return "Info";
        case Logger::LOG_WARNING: return "Warning";
        default: return "<untyped>";
    }
}

void Logger::Log(int type, const std::string& text) {
    if (!outlog) outlog = new std::ofstream("shady-lua.log");
    if ((type & logFlags) == 0) return;
    std::time_t t = std::time(0);
    *outlog << std::ctime(&t) << " "
        << getTypeName(type) << ": "
        << text.c_str() << std::endl;
    outlog->flush();
    //logContent.push_back(text);
}
