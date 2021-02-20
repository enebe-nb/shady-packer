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
#include "../Lua/shady-lua.hpp"
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

	typedef int (WINAPI* read_constructor_t)(const char *filename, void *a, void *b);
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

static FileID repl_AddFile(const std::wstring& path) {
    std::string pathu8 = ws2s(path);
    if (std::filesystem::is_directory(path)) {
        int id = package.appendPackage(pathu8.c_str());
        applyGameFilters(id);
        return id;
    }

    std::ifstream file(pathu8);
    if (file.fail()) return -1;
    auto type = ShadyCore::FileType::get(pathu8.c_str(), file);
    file.close();

    int id;
    if (type == ShadyCore::FileType::TYPE_PACKAGE) {
        id = package.appendPackage(pathu8.c_str());
    } else {
        const char* filename = pathu8.c_str();
        id = package.appendFile(std::filesystem::path(path).filename().string().c_str(), filename);
    }

    applyGameFilters(id);
    return id;
}

static bool repl_RemoveFileByPath(const std::wstring& path) {
    std::string pathu8 = ws2s(path);
    package.detachFile(pathu8.c_str());
    return true;
}

static bool repl_RemoveFile(FileID id) {
    if (id < 0) return false;
    package.detach(id);
    return true;
}

static bool repl_RemoveAllFiles() {
    package.clear();
    return true;
}

static int WINAPI repl_read_constructor(const char *filename, void *a, void *b) {
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
            size_t size = strlen(filenameB);
            char* filename = new char[size + 1];
            strcpy(filename, filenameB);
            strcpy(filename + size - 4, type.inverseExt);
            iter = package.findFile(filename);
            delete[] filename;
        }
    }

    int result;
    if (iter != package.end()) {
        auto type = ShadyCore::FileType::get(*iter);
        if (type.isEncrypted || type == ShadyCore::FileType::TYPE_UNKNOWN) {
            result = setup_entry_reader(esi_value, *iter);
        } else {
            std::istream& input = iter->open();
            std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
            ShadyCore::convertResource(type, input, *buffer);
            size_t size = buffer->tellp();
            iter->close();

            result = setup_stream_reader(esi_value, buffer, size);
        }
    } else {
        __asm mov esi, esi_value
		result = orig_read_constructor(filename, a, b);
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
    if (!iniConfig.useIntercept) return;

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

void LoadIntercept() {
    HMODULE handle = GetModuleHandle(TEXT("SokuEngine.dll"));
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
}

void UnloadIntercept() {
    package.clear();

    HMODULE handle = GetModuleHandle(TEXT("SokuEngine.dll"));
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

namespace {
    struct membuf : std::streambuf {
        membuf(char* begin, char* end) {
            this->setg(begin, begin, end);
            this->setp(begin, begin, end);
        }

        pos_type seekoff(off_type off,
                 std::ios_base::seekdir dir,
                 std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override {
            if (dir == std::ios_base::cur) {
                if (which & std::ios_base::in) gbump(off);
                if (which & std::ios_base::out) pbump(off);
            } else if (dir == std::ios_base::end) {
                if (which & std::ios_base::in) setg(eback(), egptr() + off, egptr());
                if (which & std::ios_base::out) setp(pbase(), epptr() + off, epptr());
            } else if (dir == std::ios_base::beg) {
                if (which & std::ios_base::in) setg(eback(), eback() + off, egptr());
                if (which & std::ios_base::out) setg(pbase(), pbase() + off, epptr());
            } return which & std::ios_base::in ? gptr() - eback() : pptr() - pbase();
        }

        pos_type seekpos(pos_type sp, std::ios_base::openmode which) override {
            return seekoff(sp - pos_type(off_type(0)), std::ios_base::beg, which);
        }
    };
}

static char* convertSoundLabel(char*& data, int& size) {
    membuf inputBuf(data, data + size);
    std::istream input(&inputBuf);
    ShadyCore::LabelResource label;
    ShadyCore::readResource(&label, input, false);
    delete[] data;

    size = 0x61 + strlen(label.getName());
    data = new char[size];
    membuf outputBuf(data, data + size);
    std::ostream output(&outputBuf);
    ShadyCore::writeResource(&label, output, true);
    return data;
}

static uint16_t packColor(uint32_t color, bool transparent) {
	uint16_t pcolor = !transparent;
	pcolor = (pcolor << 5) + ((color >> 3) & 0x1F);
	pcolor = (pcolor << 5) + ((color >> 11) & 0x1F);
	pcolor = (pcolor << 5) + ((color >> 19) & 0x1F);
	return pcolor;
}

static char* convertPalette(char*& data, int& size) {
	for (int i = 0; i < 256; ++i) {
		*(uint16_t*)&data[i * 2 + 1] = packColor(*(uint32_t*)&data[i * 3], i == 0);
	}

    data[0] = 0x10;
    return data;
}

static char* convertSound(char*& data, int& size) {
    membuf inputBuf(data, data + size);
    std::istream input(&inputBuf);
    ShadyUtil::RiffDocument riff(input);

    size = 0x16 + riff.size("WAVEdata");
    char* output = new char[size];
	riff.read("WAVEfmt ", (uint8_t*)output);
    output[16] = 0; output[17] = 0;
    *(uint32_t*)(output + 18) = size - 0x16;
	riff.read("WAVEdata", (uint8_t*)output + 22);

    delete[] data;
    return output;
}

static char* convertComplex(char*& data, int& size, ShadyCore::Resource* resource) {
    membuf buffer(data, data + size);
    std::istream input(&buffer);
    ShadyCore::readResource(resource, input, false);
    std::ostream output(&buffer);
    ShadyCore::writeResource(resource, output, true);
    size = output.tellp(); // always smaller
    delete resource;

    return data;
}

void LoadConverter() {
    Soku::AddCustomConverter(".lbl", ".sfl", {convertSoundLabel});
    Soku::AddCustomConverter(".act", ".pal", {convertPalette});
    Soku::AddCustomConverter(".wav", ".cv3", {convertSound});
    Soku::AddCustomConverter(".xml", ".dat", [](char*& data, int& size) {
        return convertComplex(data, size, new ShadyCore::GuiRoot());});
    Soku::AddCustomConverter(".xml", ".pat", [](char*& data, int& size) {
        return convertComplex(data, size, new ShadyCore::Pattern(false));});
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

void* EnablePackage(const std::filesystem::path& name, const std::filesystem::path& ext, std::vector<FileID>& sokuIds) {
	std::filesystem::path path(name); path += ext;
	if (path.is_relative()) path = ModPackage::basePath / path;
    void* script = 0;

	if (!hasSokuEngine) {
        if (std::filesystem::is_directory(path)
            || std::filesystem::is_regular_file(path)) {
            
            std::string pathu8 = ws2s(path);
            int id = package.appendPackage(pathu8.c_str());
            applyGameFilters(id);
            sokuIds.push_back(id);
        }
    } else if (iniConfig.useIntercept) {
        FileID id = Soku::AddFile(path);
		sokuIds.push_back(id);
        if (ShadyLua::isAvailable()) {
            auto iter = package.findFile("init.lua");
            if (iter != package.end() && iter->getId() == id) {
                script = ShadyLua::LoadFromGeneric(&package, _lua_open, _lua_read, "init.lua");
            }
        }
	} else if (ext == ".zip") {
		sokuIds.push_back(Soku::AddFile(path));
        if (ShadyLua::isAvailable() && ShadyLua::ZipExists(path.string().c_str(), "init.lua"))
            script = ShadyLua::LoadFromZip(path.string().c_str(), "init.lua");
	} else if (ext == ".dat") {
		char magicWord[6];
		std::ifstream input; input.open(path, std::ios::binary);
		input.unsetf(std::ios::skipws);
		input.read(magicWord, 6);
		input.close();
		if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
			sokuIds.push_back(Soku::AddFile(path));
		} else {
			reinterpret_cast<void(*)(const char *)>(0x0040D1D0u)(path.string().c_str());
		}
	} else if (std::filesystem::is_directory(path)) {
        std::filesystem::path luaRoot(path / "init.lua");

        for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
            if (std::filesystem::is_regular_file(iter->path())) {
                sokuIds.push_back(Soku::AddFile(iter->path()));
                if (ShadyLua::isAvailable() && iter->path() == luaRoot)
                    script = ShadyLua::LoadFromFilesystem(iter->path().string().c_str());
            }
        }
	}

    return script;
}

void DisablePackage(std::vector<FileID>& sokuIds, void* script) {
    if (script && hasSokuEngine && ShadyLua::isAvailable()) ShadyLua::FreeScript(script);
    for(auto& id : sokuIds) {
        if (hasSokuEngine) Soku::RemoveFile(id);
        else if (id >= 0) package.detach(id);
    } sokuIds.clear();
}