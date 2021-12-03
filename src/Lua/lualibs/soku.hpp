#pragma once

#include <string>
#include "../logger.hpp"

namespace ShadyLua {
    // TODO arguments on Emitters
    void EmitSokuEventRender();
    void EmitSokuEventBattleEvent(int eventId);
    //void EmitSokuEventGameEvent(int sceneId);
    void EmitSokuEventStageSelect(int* stageId);
    void EmitSokuEventFileLoader(const char* filename, std::istream **input, int*size);

    inline void DefaultRenderHook() {
        Logger::Render();
        EmitSokuEventRender();
    }

    void LoadTamper(const std::wstring& caller);
    void UnloadTamper();
    class LuaScript;
    void RemoveEvents(LuaScript*);
}