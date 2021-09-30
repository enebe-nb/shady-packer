#include "../lualibs.hpp"
#include "../../Core/reader.hpp"
#include <windows.h>
#include <SokuLib.hpp>

#define IMGUIMAN_IMPL
#include "soku.hpp"

namespace {
    auto __readerCreate = reinterpret_cast<void* (__stdcall *)(const char *filename, unsigned int *size, unsigned int *offset)>(0x0041c080);
    auto __loader = reinterpret_cast<int(*)()>(0);
    bool _initialized = false;

    typedef void (__fastcall* stage_load_t)(int, int, int, int);
    stage_load_t orig_stage_load = (stage_load_t)0x470e40;
    // typedef void* (__stdcall* scene_constructor_t)(int);
    // scene_constructor_t orig_scene_constructor = (scene_constructor_t)0x41e420;
    typedef void* (__fastcall* battle_event_t)(int, int, int);
    battle_event_t orig_battle_event = (battle_event_t)0x481eb0;
    typedef void* (__fastcall* render_event_t)(int);
    render_event_t orig_render_event = (render_event_t)0x401040;
    typedef void* (__fastcall* renderer_unk_t)(int, int, int);
    renderer_unk_t renderer_unk = (renderer_unk_t)0x404af0;

    int (*th123eAfterLock)(void) = 0;
}

static DWORD TrampolineCreate(DWORD addr, DWORD target, int offset) {
    auto lpAddr = (unsigned char *)addr;
    auto lpTramp = new unsigned char[offset + 5];
    DWORD dwOldProtect;

    memcpy(lpTramp, lpAddr, offset);
    lpTramp[offset] = 0xE9;
    *(int *)&lpTramp[offset + 1] = (int)addr - (int)lpTramp - 5;
    ::VirtualProtect(lpTramp, offset + 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

    ::VirtualProtect(lpAddr, offset, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *lpAddr = 0xE9;
    *(int *)(lpAddr + 1) = (int)target - (int)lpAddr - 5;
    memset(lpAddr + 5, 0x90, offset - 5);
    ::VirtualProtect(lpAddr, offset, dwOldProtect, &dwOldProtect);

    ::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    return reinterpret_cast<DWORD>(lpTramp);
}

static void TrampolineDestroy(DWORD addr, DWORD tramp, int offset) {
    auto lpAddr = (unsigned char *)addr;
    auto lpTramp = (unsigned char *)tramp;
    DWORD dwOldProtect;

    ::VirtualProtect(lpAddr, offset, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    memcpy(lpAddr, lpTramp, offset);
    ::VirtualProtect(lpAddr, offset, dwOldProtect, &dwOldProtect);

    ::VirtualProtect(lpTramp, offset + 5, PAGE_READWRITE, &dwOldProtect);
    ::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    delete[] lpTramp;
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

static void __fastcall repl_stage_load(int ecx, int edx, int stageId, int unknown) {
    ShadyLua::EmitSokuEventStageSelect(&stageId);
    orig_stage_load(ecx, edx, stageId, unknown);
}

// static void* __stdcall repl_scene_constructor(int scene) {
//     ShadyLua::EmitSokuEventGameEvent(scene);
//     return orig_scene_constructor(scene);
// }

static void* __fastcall repl_battle_event(int ecx, int edx, int eventId) {
    ShadyLua::EmitSokuEventBattleEvent(eventId);
    return orig_battle_event(ecx, edx, eventId);
}

static void* __fastcall repl_render_event(int ecx) {
    ShadyLua::EmitSokuEventRender();
    Logger::Render();
    return orig_render_event(ecx);
}

static int _HookLoader() {
    if (__loader) __loader();
    DWORD dwOldProtect;
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    __readerCreate = SokuLib::TamperNearJmpOpr(0x0040D227, readerCreate);
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    _initialized = true;
    // set EAX to restore hooked instruction
    return *(int*)0x008943b8;
}

void ShadyLua::LoadTamper(const std::wstring& caller) {
    DWORD dwOldProtect;
    if (caller == L"SokuEngine.dll") _HookLoader();
    else {
        DWORD dwOldProtect;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
        bool hasCall = 0xE8 == *(unsigned char*)0x007fb596;
        __loader = SokuLib::TamperNearCall(0x007fb596, _HookLoader);
        if (!hasCall) __loader = 0;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
    }

    // stage select
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_stage_load = (stage_load_t) SokuLib::TamperNearJmpOpr(0x0047157e, reinterpret_cast<DWORD>(repl_stage_load));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, dwOldProtect, &dwOldProtect);

    // game event
    // orig_scene_constructor = (scene_constructor_t) TrampolineCreate(0x0041e420, reinterpret_cast<DWORD>(repl_scene_constructor), 7);

    // battle event
    orig_battle_event = (battle_event_t) TrampolineCreate(0x00481eb0, reinterpret_cast<DWORD>(repl_battle_event), 7);

    // Render
    orig_render_event = (render_event_t) TrampolineCreate(0x00401040, reinterpret_cast<DWORD>(repl_render_event), 5);
    //new std::thread(ImGuiMan::HookThread, load, render);
}

void ShadyLua::UnloadTamper() {
    DWORD dwOldProtect;

    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    SokuLib::TamperNearJmpOpr(0x0047157e, reinterpret_cast<DWORD>(orig_stage_load));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, dwOldProtect, &dwOldProtect);

    // ::VirtualProtect(reinterpret_cast<LPVOID>(0x0041e420), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    // TrampolineDestroy(0x0041e420, reinterpret_cast<DWORD>(orig_scene_constructor), 7);
    // ::VirtualProtect(reinterpret_cast<LPVOID>(0x0041e420), 5, dwOldProtect, &dwOldProtect);

    TrampolineDestroy(0x00481eb0, reinterpret_cast<DWORD>(orig_battle_event), 7);

    TrampolineDestroy(0x00401040, reinterpret_cast<DWORD>(orig_render_event), 5);

    if (!_initialized) return;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    SokuLib::TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(__readerCreate));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);
}
