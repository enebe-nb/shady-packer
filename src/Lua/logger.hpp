#pragma once

#include <string>

namespace Logger {
    constexpr int LOG_DEBUG =   1 << 0;
    constexpr int LOG_ERROR =   1 << 1;
    constexpr int LOG_INFO =    1 << 2;
    constexpr int LOG_WARNING = 1 << 3;
    constexpr int LOG_ALL =     0x0f;
    void Initialize(int flags = LOG_ERROR, const std::string_view& filename = "");
    void Finalize();
    void Render();
    void Clear();

    void Log(int type, const std::string&);
    template<typename ... S> inline void Log(int type, S const&... args) {
        std::string concatened;
        int dummy[] = {0, ((concatened += args), 0)...}; Log(type, concatened);
    }
    template<typename ... S> inline void Debug(S const&... right) {Log(LOG_DEBUG, right...);}
    template<typename ... S> inline void Error(S const&... right) {Log(LOG_ERROR, right...);}
    template<typename ... S> inline void Info(S const&... right) {Log(LOG_INFO, right...);}
    template<typename ... S> inline void Warning(S const&... right) {Log(LOG_WARNING, right...);}
}