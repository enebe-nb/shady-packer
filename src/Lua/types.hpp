#pragma once

namespace ShadyLua {
    typedef void* (*fnOpen_t)(void* userdata, const char* filename);
    typedef size_t (*fnRead_t)(void* userdata, void* file, char* buffer, size_t size);
}
