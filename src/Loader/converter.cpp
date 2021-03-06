#include "main.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <stack>

#include "../Core/package.hpp"
#include "../Core/util/riffdocument.hpp"
#include "../Core/reader.hpp"
#include "ModPackage.hpp"
#include "../Lua/lualibs/soku.hpp"

#define TEXT_SECTION_OFFSET  0x00401000
#define TEXT_SECTION_SIZE    0x00445000

ShadyCore::Package package;
namespace {
	typedef void* (__stdcall* read_constructor_t)(const char *filename, unsigned int *size, unsigned int *offset);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;
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
/*
namespace {
    typedef struct {
        unsigned char lc = 0;
        unsigned char operator()(unsigned char c) {
            if (lc >= 0x80 && lc <= 0xa0 || lc >= 0xe0 && lc <= 0xff) {
                lc = 0;
                return c;
            }
            lc = c;
            if (c >= 0x41 && c <= 0x5a) {
                return c + 0x20;
            }
            return c;
        }
        void operator()(char* dst, const char* src) {
            while(*src) {
                *dst++ = this->operator()(*src++);
            }
        }
    } sjis2lower;
}
*/

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
        //sjis2lower tolower;
		if (filename[i] == '/') filenameB[i] = '_';
		else filenameB[i] = tolower(filename[i]);
	} filenameB[len] = 0;

    loadLock.lock_shared();
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
    delete[] filenameB;

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

    return result;
}

static int _LoadTamper() {
    if (th123eAfterLock) th123eAfterLock();
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_read_constructor = (read_constructor_t) TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(repl_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);
    return *(int*)0x008943b8;
}

void LoadTamper(const std::wstring& caller) {
    ShadyLua::LoadTamper(caller);
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

void UnloadTamper() {
    ShadyLua::UnloadTamper();
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);
}