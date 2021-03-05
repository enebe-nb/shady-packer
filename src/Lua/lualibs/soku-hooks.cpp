#include "soku.hpp"
#include "../lualibs.hpp"
#include "../../Loader/reader.hpp"
#include <windows.h>

namespace {
	typedef void* (__stdcall* read_constructor_t)(const char *filename, unsigned int *size, unsigned int *offset);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;
    typedef void (__fastcall* stage_load_t)(int, int, int, int);
    stage_load_t orig_stage_load = (stage_load_t)0x470e40;

    int (*th123eAfterLock)(void) = 0;
}


inline DWORD TamperNearJmpOpr(DWORD addr, DWORD target) {
    DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
    *reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
    return old;
}

inline DWORD TamperNearCall(DWORD addr, DWORD target) {
    *reinterpret_cast<PBYTE>(addr + 0) = 0xE8;
    return TamperNearJmpOpr(addr, target);
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
    ShadyLua::EmitSokuEventStageSelect(stageId);
    orig_stage_load(ecx, edx, stageId, unknown);
}

static int _LoadTamper() {
    if (th123eAfterLock) th123eAfterLock();
    DWORD dwOldProtect;

    // file loader
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_read_constructor = (read_constructor_t) TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(repl_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);

    // stage select
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_stage_load = (stage_load_t) TamperNearJmpOpr(0x0047157e, reinterpret_cast<DWORD>(repl_stage_load));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, dwOldProtect, &dwOldProtect);
    return *(int*)0x008943b8;
}

void ShadyLua::LoadTamper(const std::wstring& caller) {
    if (caller == L"SokuEngine.dll") _LoadTamper();
    else {
        DWORD dwOldProtect;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
        bool hasCall = 0xE8 == *(unsigned char*)0x007fb596;
        th123eAfterLock = (int(*)(void)) TamperNearCall(0x007fb596, reinterpret_cast<DWORD>(_LoadTamper));
        if (!hasCall) th123eAfterLock = 0;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
    }
}

void ShadyLua::UnloadTamper() {
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);

    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0047157e, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0047157e), 5, dwOldProtect, &dwOldProtect);
}
