#pragma once

#include <string>

namespace ShadyLua {
    // TODO arguments on Emitters
    void EmitSokuEventRender();
    void EmitSokuEventBattleEvent();
    void EmitSokuEventGameEvent();
    void EmitSokuEventStageSelect(int stageId);
    void EmitSokuEventFileLoader(const char* filename, std::istream **input, int*size);

    void LoadTamper(const std::wstring& caller);
    void UnloadTamper();
}