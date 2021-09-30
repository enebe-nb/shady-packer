#pragma once
#include <SokuLib.hpp>
#include <deque>
#include <fstream>

namespace SokuLib {
    class InputHandler {
    private:
        int max, unknown04;
        const int* valueAddr;
    public:
        int pos, unknown10;

        static inline const int* keyLeftRight() { return (*(const int**)0x89a394) + 14; }
        static inline const int* keyUpDown()    { return (*(const int**)0x89a394) + 15; }
        static inline const int* keyA()         { return (*(const int**)0x89a394) + 16; }
        static inline const int* keyB()         { return (*(const int**)0x89a394) + 17; }
        static inline const int* keyC()         { return (*(const int**)0x89a394) + 18; }
        static inline const int* keyD()         { return (*(const int**)0x89a394) + 19; }
        static inline const int* keyE()         { return (*(const int**)0x89a394) + 20; }
        static inline const int* keyF()         { return (*(const int**)0x89a394) + 21; }

        inline void set(const int* valueAddr, int pos = 0, int max = 1) {
            this->max = max; this->unknown04 = 0;
            this->pos = this->unknown10 = pos;
            this->valueAddr = valueAddr;
        }
        inline bool update() { return (this->*union_cast<bool (InputHandler::*)()>(0x41fbf0))(); }

        static void renderBar(float x, float y, float width) { union_cast<void(*)(float, float, float)>(0x443a50)(x, y, width); }
    };
}