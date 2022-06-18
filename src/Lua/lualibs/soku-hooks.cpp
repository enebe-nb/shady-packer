#include "../lualibs.hpp"
#include "../../Core/reader.hpp"
#include <windows.h>
#include <SokuLib.hpp>

#define IMGUIMAN_IMPL
#include "soku.hpp"

namespace {
    auto _eventPlayerInfo = reinterpret_cast<void (__fastcall *)(void* unknown, int unused, int index, const SokuLib::PlayerInfo& info)>(0x0046da40);
    auto __readerCreate = reinterpret_cast<void* (__stdcall *)(const char *filename, unsigned int *size, unsigned int *offset)>(0x0041c080);
    auto __loader = reinterpret_cast<int(*)()>(0);
    bool _initialized = false;

    SokuLib::Trampoline* _renderEvent;
}

static void* __stdcall readerCreate(const char *filename, unsigned int *_size, unsigned int *_offset) {
    int esi_value;
	__asm mov esi_value, esi

    size_t len = strlen(filename);
	char* filenameB = new char[len + 1];
	for (int i = 0; i < len; ++i) {
        if (filename[i] >= 0x80 && filename[i] <= 0xa0 || filename[i] >= 0xe0 && filename[i] <= 0xff) {
            // sjis character (those stand pictures)
            filenameB[i] = filename[i];
            ++i; filenameB[i] = filename[i];
        } else if (filename[i] == '/') filenameB[i] = '_';
		else filenameB[i] = std::tolower(filename[i]);
	} filenameB[len] = 0;

    std::istream* input = 0; int s;
    ShadyLua::EmitSokuEventFileLoader(filenameB, &input, &s);
    if (!input) ShadyLua::EmitSokuEventFileLoader2(filenameB, &input, &s);
    delete[] filenameB;

    if (input) {
        *_offset = 0x40000000; // just to hold a value
        *(int*)esi_value = ShadyCore::stream_reader_vtbl;
        *_size = s;
        return input;
    } else {
        // return __readerCreate(filename, _size, _offset);
        __asm {
            push _offset;
            push _size;
            push filename;
	        mov esi, esi_value;
            call __readerCreate;
            mov esi_value, eax;
        } return (void*)esi_value;
    }
}

static void renderEvent() {
    ShadyLua::EmitSokuEventRender();
    Logger::Render();
}

static void __fastcall eventPlayerInfo(void* unknown, int unused, int index, const SokuLib::PlayerInfo& info) {
    ShadyLua::EmitSokuEventPlayerInfo(info);
    return _eventPlayerInfo(unknown, unused, index, info);
}

static int _HookLoader() {
    if (__loader) __loader();
    _renderEvent = new SokuLib::Trampoline(0x00401040, renderEvent, 5);


    DWORD dwOldProtect;
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    __readerCreate = SokuLib::TamperNearJmpOpr(0x0040D227, readerCreate);
    _eventPlayerInfo = SokuLib::TamperNearJmpOpr(0x46eb1b, eventPlayerInfo);
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);

    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    _initialized = true;
    // set EAX to restore hooked instruction
    return *(int*)0x008943b8;
}

void ShadyLua::LoadTamper() {
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    bool hasCall = 0xE8 == *(unsigned char*)0x007fb596;
    __loader = SokuLib::TamperNearCall(0x007fb596, _HookLoader);
    if (!hasCall) __loader = 0;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
}

void ShadyLua::UnloadTamper() {
    DWORD dwOldProtect;

    if (!_initialized) return;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    SokuLib::TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(__readerCreate));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);

    delete _renderEvent;
}
