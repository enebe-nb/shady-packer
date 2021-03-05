#pragma once

#include <string>

namespace ShadyLua {
    // TODO arguments on Emitters
    void EmitSokuEventRender();
    void EmitSokuEventBattleEvent(int eventId);
    //void EmitSokuEventGameEvent(int sceneId);
    void EmitSokuEventStageSelect(int* stageId);
    void EmitSokuEventFileLoader(const char* filename, std::istream **input, int*size);

    void LoadTamper(const std::wstring& caller);
    void UnloadTamper();
}