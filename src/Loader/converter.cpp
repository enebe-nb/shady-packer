#include "main.hpp"

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stack>

#include "../Core/package.hpp"
#include "../Core/util/riffdocument.hpp"
#include "reader.hpp"
#include "ModPackage.hpp"
#include "../Lua/lualibs.hpp"

#define TEXT_SECTION_OFFSET  0x00401000
#define TEXT_SECTION_SIZE    0x00445000

ShadyCore::Package package;
namespace {
	typedef void* (__stdcall* read_constructor_t)(const char *filename, unsigned int *size, unsigned int *offset);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;
}

inline DWORD TamperNearJmpOpr(DWORD addr, DWORD target) {
    DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
    *reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
    return old;
}

inline void TamperNearCall(DWORD addr, DWORD target) {
    *reinterpret_cast<PBYTE>(addr + 0) = 0xE8;
    TamperNearJmpOpr(addr, target);
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

    loadLock.lock_shared();
    {
        std::istream* input = 0; int s;
        ShadyLua::EmitSokuEventFileLoader(filenameB, &input, &s);
        if (input) {
            *(int*)esi_value = ShadyCore::stream_reader_vtbl;
            *size = s;
            loadLock.unlock_shared();
            delete[] filenameB;
            return input;
        }
    }

    auto iter = package.findFile(filenameB);
    if (iter == package.end()) {
        auto type = ShadyCore::FileType::getSimple(filenameB);
        if (type.type != ShadyCore::FileType::TYPE_UNKNOWN
            && type.type != ShadyCore::FileType::TYPE_PACKAGE) {
            char* filenameC = new char[len + 1];
            strcpy(filenameC, filenameB);
            strcpy(filenameC + len - 4, type.inverseExt);
            iter = package.findFile(filenameC);
            delete[] filenameC;
        }
    }

    void* result;
    if (iter != package.end()) {
        auto type = ShadyCore::FileType::get(*iter);
        *offset = 0x40000000; // just to hold a value
        if (type.isEncrypted || type == ShadyCore::FileType::TYPE_UNKNOWN) {
            *size = iter->getSize();
            *(int*)esi_value = ShadyCore::entry_reader_vtbl;
            result = new ShadyCore::EntryReader(*iter, loadLock);
        } else {
            std::istream& input = iter->open();
            std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
            ShadyCore::convertResource(type, input, *buffer);
            *size = buffer->tellp();
            iter->close();

            *(int*)esi_value = ShadyCore::stream_reader_vtbl;
            result = buffer;
        }
    } else {
#ifdef _MSC_VER
        // Required for th123e
	    __asm mov esi, esi_value
#else
        asm("mov %0, %%esi" :: "rmi"(esi_value));
#endif
		result = orig_read_constructor(filename, size, offset);
    }
    loadLock.unlock_shared();

    delete[] filenameB;
    return result;
}

int _LoadTamper() {
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_read_constructor = (read_constructor_t) TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(repl_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);
    return *(int*)0x008943b8;
}

void LoadTamper(const std::filesystem::path& caller) {
    if (caller == L"SokuEngine.dll") _LoadTamper();
    else {
        DWORD dwOldProtect;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
        TamperNearCall(0x007fb596, reinterpret_cast<DWORD>(_LoadTamper));
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
    }
}

void UnloadTamper() {
    package.clear();

    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);
}