#include "main.hpp"

#include <shlwapi.h>
#include <SokuLib.h>
#include <string>
#include <fstream>
#include <sstream>

#include "../Core/package.hpp"

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
    EventID fileLoaderEvent;

    typedef void (WINAPI* orig_dealloc_t)(int);
	orig_dealloc_t orig_dealloc = (orig_dealloc_t)0x81f6fa;
    struct reader_data {
        ShadyCore::BasePackageEntry* entry;
        std::istream* stream;
        int size;
        int bytes_read;
    };
}

std::string ws2s(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),&strTo[0], size_needed, NULL, NULL);
    return strTo;
}

DWORD DoIntercept(DWORD addr, DWORD target, int offset) {
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

void UndoIntercept(DWORD addr, DWORD tramp, int offset) {
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

FileID repl_AddFile(const std::wstring& path) {
    std::string pathu8 = ws2s(path);
    DWORD attr = GetFileAttributes(pathu8.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return -1;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        outlog << "Dir " << pathu8 << std::endl;
        int id = package.appendPackage(pathu8.c_str());
        applyGameFilters(id);
        return id;
    }

    std::ifstream file(pathu8);
    if (file.fail()) return -1;
    int id;
    auto type = ShadyCore::FileType::get(pathu8.c_str(), file);
    file.close();
    if (type == ShadyCore::FileType::TYPE_PACKAGE) {
        outlog << "Pack " << pathu8 << std::endl;
        id = package.appendPackage(pathu8.c_str());
    } else {
        outlog << "File " << pathu8 << std::endl;
        const char* filename = pathu8.c_str();
        id = package.appendFile(PathFindFileName(filename), filename);
    }

    applyGameFilters(id);
    return id;
}

bool repl_RemoveFileByPath(const std::wstring& path) {
    std::string pathu8 = ws2s(path);
    package.detachFile(pathu8.c_str());
    return true;
}

bool repl_RemoveFile(FileID id) {
    if (id < 0) return false;
    package.detach(id);
    return true;
}

bool repl_RemoveAllFiles() {
    package.clear();
    return true;
}

static int WINAPI reader_destruct(int a) {
	int ecx_value;
	__asm mov ecx_value, ecx
	if (!ecx_value) return 0;

    reader_data *data = (reader_data *)(ecx_value+4);
    if (data->size == 0) data->entry->close();
    else delete data->stream;
    if (a & 1) orig_dealloc(ecx_value);
	return ecx_value;
}

static int WINAPI reader_read(char *dest, int size) {
	int ecx_value;
	__asm mov ecx_value, ecx
	if (!ecx_value) return 0;

	reader_data *data = (reader_data *)(ecx_value + 4);
    if ((void*)data->entry == (void*)data->stream && data->size == 0) data->stream = &data->entry->open();
	data->stream->read(dest, size);
    data->bytes_read = data->stream->gcount();
	return data->bytes_read > 0;
}

static void WINAPI reader_seek(int offset, int whence) {
	int ecx_value;
	__asm mov ecx_value, ecx
	if (ecx_value == 0) return;

	reader_data *data = (reader_data *)(ecx_value + 4);
    if ((void*)data->entry == (void*)data->stream && data->size == 0) data->stream = &data->entry->open();
	switch(whence) {
	case 0:
		if (offset > data->size) data->stream->seekg(0, std::ios::end);
		else data->stream->seekg(offset, std::ios::beg);
		break;
	case 1:
		if ((int)data->stream->tellg() + offset > data->size) {
			data->stream->seekg(0, std::ios::end);
		} else if ((int)data->stream->tellg() + offset < 0) {
			data->stream->seekg(0, std::ios::beg);
		} else data->stream->seekg(offset, std::ios::cur);
		break;
	case 2:
		if (offset > data->size) data->stream->seekg(0, std::ios::beg);
		else data->stream->seekg(-offset, std::ios::end);
		break;
	}
}

static int WINAPI reader_getSize() {
	int ecx_value;
	__asm mov ecx_value, ecx
	if (ecx_value == 0) return 0;

	reader_data *data = (reader_data *)(ecx_value + 4);
	return data->size;
}

static int WINAPI reader_getRead() {
	int ecx_value;
	__asm mov ecx_value, ecx
	if (ecx_value == 0) return 0;

	reader_data *data = (reader_data *)(ecx_value + 4);
	return data->bytes_read;
}

functable reader_table = {
    reader_destruct,
	reader_read,
	reader_getRead,
	reader_seek,
	reader_getSize
};

void FileLoaderCallback(SokuData::FileLoaderData& data) {
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
        outlog << data.fileName << std::endl;
        outlog.flush();
        auto type = ShadyCore::FileType::get(*iter);
        if (type.isEncrypted) {
            data.inputFormat = DataFormat::RAW;
            data.data = &*iter;
            data.size = 0;
            data.reader = &reader_table;
        } else if (type == ShadyCore::FileType::TYPE_IMAGE) {
            data.inputFormat = DataFormat::PNG;
            std::istream& input = iter->open();
            input.seekg(0, std::ios::end);
            data.size = input.tellg();
            data.data = new char[data.size];
            input.seekg(0, std::ios::beg);
            input.read((char*)data.data, data.size);
            iter->close();
        } else {
            std::istream& input = iter->open();
            std::stringstream* buffer = new std::stringstream();
            ShadyCore::convertResource(type, input, *buffer);
            data.size = buffer->tellp();
            outlog << "tellp " << data.size << std::endl;
            iter->close();

            data.inputFormat = DataFormat::RAW;
            data.data = buffer;
            data.reader = &reader_table;
        }
    }
}

void LoadFileLoader() {
    HMODULE handle = GetModuleHandle("SokuEngine.dll");
	if (handle != INVALID_HANDLE_VALUE) {
        // AddFile(filename)
        DWORD addr = (DWORD) GetProcAddress(handle, "?AddFile@Soku@@YAHABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z");
        tramp_AddFile = (AddFile_t) DoIntercept(addr, (DWORD)&repl_AddFile, 5);
        // RemoveFile(id)
        addr = (DWORD) GetProcAddress(handle, "?RemoveFile@Soku@@YA_NABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z");
        tramp_RemoveFileByPath = (RemoveFileByPath_t) DoIntercept(addr, (DWORD)&repl_RemoveFileByPath, 5);
        // RemoveFile(id)
        addr = (DWORD) GetProcAddress(handle, "?RemoveFile@Soku@@YA_NH@Z");
        tramp_RemoveFile = (RemoveFile_t) DoIntercept(addr, (DWORD)&repl_RemoveFile, 6);
        // RemoveAllFiles(id)
        addr = (DWORD) GetProcAddress(handle, "?RemoveAllFiles@Soku@@YA_NXZ");
        tramp_RemoveAllFiles = (RemoveAllFiles_t) DoIntercept(addr, (DWORD)&repl_RemoveAllFiles, 6);
	}

    fileLoaderEvent = Soku::SubscribeEvent(SokuEvent::FileLoader, {FileLoaderCallback});
}

void UnloadFileLoader() {
    Soku::UnsubscribeEvent(fileLoaderEvent);

    HMODULE handle = GetModuleHandle("SokuEngine.dll");
	if (handle != INVALID_HANDLE_VALUE) {
        // AddFile(filename)
        DWORD addr = (DWORD) GetProcAddress(handle, "?AddFile@Soku@@YAHABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z");
        UndoIntercept(addr, (DWORD)tramp_AddFile, 5);
        // RemoveFile(id)
        addr = (DWORD) GetProcAddress(handle, "?RemoveFile@Soku@@YA_NABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z");
        UndoIntercept(addr, (DWORD)tramp_RemoveFileByPath, 5);
        // RemoveFile(id)
        addr = (DWORD) GetProcAddress(handle, "?RemoveFile@Soku@@YA_NH@Z");
        UndoIntercept(addr, (DWORD)tramp_RemoveFile, 6);
        // RemoveAllFiles(id)
        addr = (DWORD) GetProcAddress(handle, "?RemoveAllFiles@Soku@@YA_NXZ");
        UndoIntercept(addr, (DWORD)tramp_RemoveAllFiles, 6);
    }
}
