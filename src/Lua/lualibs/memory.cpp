#include "../lualibs.hpp"
#include <LuaBridge/LuaBridge.h>
#include <windows.h>

using namespace luabridge;

/** Read from memory into a string */
static std::string memory_readbytes(int address, int size) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    std::string value((char*)address, size);
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, dwOldProtect, &dwOldProtect);
    return value;
}

/** Read a double from memory */
static double memory_readdouble(int address) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    double value = *(double*)address;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, dwOldProtect, &dwOldProtect);
    return value;
}

/** Read a float from memory */
static float memory_readfloat(int address) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    float value = *(float*)address;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, dwOldProtect, &dwOldProtect);
    return value;
}

/** Writes a string into memory */
static void memory_writebytes(int address, std::string value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), value.size(), PAGE_EXECUTE_READWRITE, &dwOldProtect);
    memcpy((void*)address, value.c_str(), value.size());
    VirtualProtect(reinterpret_cast<LPVOID>(address), value.size(), dwOldProtect, &dwOldProtect);
}

/** Writes a double into memory */
static void memory_writedouble(int address, double value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(double*)address = value;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 8, dwOldProtect, &dwOldProtect);
}

/** Writes a float into memory */
static void memory_writefloat(int address, float value) {
    DWORD dwOldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(float*)address = value;
    VirtualProtect(reinterpret_cast<LPVOID>(address), 4, dwOldProtect, &dwOldProtect);
}

void ShadyLua::LualibMemory(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("memory")
            .addFunction("readbytes", memory_readbytes)
            .addFunction("readdouble", memory_readdouble)
            .addFunction("readfloat", memory_readfloat)
            .addFunction("writebytes", memory_writebytes)
            .addFunction("writedouble", memory_writedouble)
            .addFunction("writefloat", memory_writefloat)
        .endNamespace()
    ;
}