#include "main.hpp"

#include <shlwapi.h>
#include <SokuLib.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stack>

#include "../Core/package.hpp"
#include "../Core/util/riffdocument.hpp"
#include "reader.hpp"
#include "ModPackage.hpp"

#define TEXT_SECTION_OFFSET  0x00401000
#define TEXT_SECTION_SIZE    0x00445000

namespace {
    typedef FileID (*AddFile_t)(const std::wstring& path);
    AddFile_t tramp_AddFile;
    typedef bool (*RemoveFileByPath_t)(const std::wstring& path);
    RemoveFileByPath_t tramp_RemoveFileByPath;
    typedef bool (*RemoveFile_t)(FileID);
    RemoveFile_t tramp_RemoveFile;
    typedef bool (*RemoveAllFiles_t)();
    RemoveAllFiles_t tramp_RemoveAllFiles;
    ShadyCore::Package package;

	typedef int (WINAPI* read_constructor_t)(const char *filename, unsigned int *size, unsigned int *offset);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;
}

inline DWORD TamperNearJmpOpr(DWORD addr, DWORD target) {
    DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
    *reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
    return old;
}

static DWORD DoIntercept(DWORD addr, DWORD target, int offset) {
    char* lpAddr = (char*)addr;
    char* lpTramp = new char[offset + 5];
    DWORD tramp = (DWORD) lpTramp;
    DWORD dwOldProtect;

    for (int i = 0; i < offset; ++i) *lpTramp++ = *lpAddr++;
    *lpTramp++ = 0xE9;
    *(int*)lpTramp = (int)addr - (int)tramp - 5;
    VirtualProtect(reinterpret_cast<LPVOID>(tramp), offset+5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

    VirtualProtect(reinterpret_cast<LPVOID>(addr), offset, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    *(char*)addr = 0xE9;
    *(int*)(addr + 1) = (int)target - (int)addr - 5;
    VirtualProtect(reinterpret_cast<LPVOID>(addr), offset, dwOldProtect, &dwOldProtect);

    FlushInstructionCache(GetCurrentProcess(), 0, 0);
    return tramp;
}

static void UndoIntercept(DWORD addr, DWORD tramp, int offset) {
    char* lpAddr = (char*)addr;
    char* lpTramp = (char*)tramp;
    DWORD dwOldProtect;

    ::VirtualProtect(reinterpret_cast<LPVOID>(addr), offset, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    for (int i = 0; i < offset; ++i) *lpAddr++ = *lpTramp++;
    ::VirtualProtect(reinterpret_cast<LPVOID>(addr), offset, dwOldProtect, &dwOldProtect);

    delete[] (char*)tramp;
    FlushInstructionCache(GetCurrentProcess(), 0, 0);
}

static void applyGameFilters(int id = -1) {
    ShadyCore::PackageFilter::apply(package, 
        (ShadyCore::PackageFilter::Filter)(
            ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION |
            ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE |
            ShadyCore::PackageFilter::FILTER_TO_LOWERCASE
        ), id);
}

static int WINAPI repl_read_constructor(const char *filename, unsigned int *size, unsigned int *offset) {
    int esi_value;
	__asm mov esi_value, esi

    size_t len = strlen(filename);
	char* filenameB = new char[len + 1];
	char* extension = 0;
	for (int i = 0; i < len; ++i) {
		if (filename[i] == '/') filenameB[i] = '_';
		else filenameB[i] = tolower(filename[i]);
		if (filenameB[i] == '.') extension = &filenameB[i];
	} filenameB[len] = 0;

    auto iter = package.findFile(filenameB);
    if (iter == package.end()) {
        auto type = ShadyCore::FileType::getSimple(filenameB);
        if (type.type != ShadyCore::FileType::TYPE_UNKNOWN) {
            char* filename = new char[len + 1];
            strcpy(filename, filenameB);
            strcpy(filename + len - 4, type.inverseExt);
            iter = package.findFile(filename);
            delete[] filename;
        }
    }

    int result;
    if (iter != package.end()) {
        auto type = ShadyCore::FileType::get(*iter);
        *offset = 0;
        if (type.isEncrypted || type == ShadyCore::FileType::TYPE_UNKNOWN) {
            *size = iter->getSize();
            result = setup_entry_reader(esi_value, *iter);
        } else {
            std::istream& input = iter->open();
            std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
            ShadyCore::convertResource(type, input, *buffer);
            *size = buffer->tellp();
            iter->close();

            result = setup_stream_reader(esi_value, buffer, *size);
        }
    } else {
        __asm mov esi, esi_value
		result = orig_read_constructor(filename, size, offset);
    }

    delete[] filenameB;
    return result;
}

void LoadTamper() {
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    orig_read_constructor = (read_constructor_t) TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(repl_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);
}

void UnloadTamper() {
    package.clear();

    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    TamperNearJmpOpr(0x0040D227, reinterpret_cast<DWORD>(orig_read_constructor));
    ::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);
}

void FileLoaderCallback(SokuData::FileLoaderData& data) {
    const char* ext = std::strrchr(data.fileName, '.');
	if (!ext || strcmp(ext, ".lua") != 0)
        {std::lock_guard<std::mutex> lock(loadLock);}

    auto iter = package.findFile(data.fileName);
    if (iter == package.end()) {
        auto type = ShadyCore::FileType::getSimple(data.fileName);
        if (type.type != ShadyCore::FileType::TYPE_UNKNOWN) {
            size_t size = strlen(data.fileName);
            char* filename = new char[size + 1];
            strcpy(filename, data.fileName);
            strcpy(filename + size - 4, type.inverseExt);
            iter = package.findFile(filename);
            delete[] filename;
        }
    }

    if (iter != package.end()) {
        auto type = ShadyCore::FileType::get(*iter);
        if (type.isEncrypted || type == ShadyCore::FileType::TYPE_UNKNOWN) {
            data.inputFormat = DataFormat::RAW;
            setup_entry_reader(data, *iter);
        } else if (type == ShadyCore::FileType::TYPE_IMAGE) {
            data.inputFormat = DataFormat::PNG;
            std::istream& input = iter->open();
            data.size = iter->getSize();
            data.data = new char[data.size];
            input.read((char*)data.data, data.size);
            iter->close();
        } else {
            std::istream& input = iter->open();
            std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
            ShadyCore::convertResource(type, input, *buffer);
            size_t size = buffer->tellp();
            iter->close();

            data.inputFormat = DataFormat::RAW;
            setup_stream_reader(data, buffer, size);
        }
    }
}

namespace {
    struct _lua_file {
        ShadyCore::BasePackageEntry* entry;
        std::istream* input;
    };
}

void* _lua_open(void* userdata, const char* filename) {
    ShadyCore::Package* package = reinterpret_cast<ShadyCore::Package*>(userdata);
    auto iter = package->findFile(filename);
    if (iter == package->end()) return 0;
    return new _lua_file{&*iter, &iter->open()};
}

size_t _lua_read(void* userdata, void* file, char* buffer, size_t size) {
    if (!file) return 0;
    _lua_file* luaFile = reinterpret_cast<_lua_file*>(file);
    luaFile->input->read(buffer, size);
    size = luaFile->input->gcount();
    if (size == 0) {
        luaFile->entry->close();
        delete luaFile;
    } return size;
}

int EnablePackage(const std::filesystem::path& name, const std::filesystem::path& ext) {
	std::filesystem::path path(name); path += ext;
	if (path.is_relative()) path = ModPackage::basePath / path;
    //void* script = 0;
    //if (ShadyLua::isAvailable()) {
        //auto iter = package.findFile("init.lua");
        //if (iter != package.end() && iter->getId() == id) {
            //script = ShadyLua::LoadFromGeneric(&package, _lua_open, _lua_read, "init.lua");
        //}
    //}

    if (std::filesystem::is_directory(path)
        || std::filesystem::is_regular_file(path)) {
        
        std::string pathu8 = ws2s(path);
        int id = package.appendPackage(pathu8.c_str());
        applyGameFilters(id);
        return id;
    }

    return -1;
}

void DisablePackage(int id, void* script) {
    //if (script && hasSokuEngine && ShadyLua::isAvailable()) ShadyLua::FreeScript(script);
    if (id >= 0) package.detach(id);
}