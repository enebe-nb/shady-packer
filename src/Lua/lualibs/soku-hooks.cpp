#include "soku.hpp"
#include "../lualibs.hpp"
#include "../../Loader/reader.hpp"
#include <windows.h>

namespace {
	typedef void* (__stdcall* read_constructor_t)(const char *filename, unsigned int *size, unsigned int *offset);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;
    typedef void (__fastcall* stage_load_t)(int, int, int, int);
    stage_load_t orig_stage_load = (stage_load_t)0x470e40;
    // typedef void* (__stdcall* scene_constructor_t)(int);
    // scene_constructor_t orig_scene_constructor = (scene_constructor_t)0x41e420;
    typedef void* (__fastcall* battle_event_t)(int, int, int);
    battle_event_t orig_battle_event = (battle_event_t)0x481eb0;

    int (*th123eAfterLock)(void) = 0;
}

static inline DWORD TamperNearJmpOpr(DWORD addr, DWORD target) {
    DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
    *reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
    return old;
}

static inline DWORD TamperNearCall(DWORD addr, DWORD target) {
    *reinterpret_cast<PBYTE>(addr + 0) = 0xE8;
    return TamperNearJmpOpr(addr, target);
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

static void* __stdcall repl_read_constructor(const char *filename, unsigned int *size, unsigned int *offset) {
#ifdef _MSC_VER
    int esi_value;
	__asm mov esi_value, esi
#else
    register int esi_value asm("esi");
#endif
    size_t len = strlen(filename);
	char* filenameB = new char[len + 1];
	for (int i = 0; i < len; ++i) {
		if (filename[i] == '/') filenameB[i] = '_';
		else filenameB[i] = tolower(filename[i]);
	} filenameB[len] = 0;

    std::istream* input = 0; int s;
    ShadyLua::EmitSokuEventFileLoader(filenameB, &input, &s);
    delete[] filenameB;

    if (input) {
        *offset = 0x40000000; // just to hold a value
        *(int*)esi_value = ShadyCore::stream_reader_vtbl;
        *size = s;
        return input;
    } else {
#ifdef _MSC_VER
	    __asm mov esi, esi_value
#else
        asm("mov %0, %%esi" :: "rmi"(esi_value));
#endif
		return orig_read_constructor(filename, size, offset);
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

static int _LoadTamper() {
    if (th123eAfterLock) th123eAfterLock();
    DWORD dwOldProtect;

    // file loader
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040d227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_read_constructor = (read_constructor_t) TamperNearJmpOpr(0x0040d227, reinterpret_cast<DWORD>(repl_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040d227), 5, dwOldProtect, &dwOldProtect);
    return *(int*)0x008943b8;
}

void ShadyLua::LoadTamper(const std::wstring& caller) {
    DWORD dwOldProtect;
    if (caller == L"SokuEngine.dll") _LoadTamper();
    else {
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
        bool hasCall = 0xE8 == *(unsigned char*)0x007fb596;
        th123eAfterLock = (int(*)(void)) TamperNearCall(0x007fb596, reinterpret_cast<DWORD>(_LoadTamper));
        if (!hasCall) th123eAfterLock = 0;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
    }

    // stage select
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_stage_load = (stage_load_t) TamperNearJmpOpr(0x0047157e, reinterpret_cast<DWORD>(repl_stage_load));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, dwOldProtect, &dwOldProtect);

    // game event
    // ::VirtualProtect(reinterpret_cast<LPVOID>(0x0041e420), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    // orig_scene_constructor = (scene_constructor_t) TrampolineCreate(0x0041e420, reinterpret_cast<DWORD>(repl_scene_constructor), 7);
    // ::VirtualProtect(reinterpret_cast<LPVOID>(0x0041e420), 5, dwOldProtect, &dwOldProtect);

    // battle event
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x00481eb0), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_battle_event = (battle_event_t) TrampolineCreate(0x00481eb0, reinterpret_cast<DWORD>(repl_battle_event), 7);
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x00481eb0), 5, dwOldProtect, &dwOldProtect);
}

void ShadyLua::UnloadTamper() {
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);

    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0047157e, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, dwOldProtect, &dwOldProtect);

    // ::VirtualProtect(reinterpret_cast<LPVOID>(0x0041e420), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    // TrampolineDestroy(0x0041e420, reinterpret_cast<DWORD>(orig_scene_constructor), 7);
    // ::VirtualProtect(reinterpret_cast<LPVOID>(0x0041e420), 5, dwOldProtect, &dwOldProtect);

    ::VirtualProtect(reinterpret_cast<LPVOID>(0x00481eb0), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TrampolineDestroy(0x00481eb0, reinterpret_cast<DWORD>(orig_battle_event), 7);
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x00481eb0), 5, dwOldProtect, &dwOldProtect);
}
