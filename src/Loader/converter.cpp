#include "main.hpp"

#include <shlwapi.h>
#include <SokuLib.h>
#include <string>
#include <fstream>
#include <sstream>
#include <stack>

#include "../Core/package.hpp"
#include "../Core/util/riffdocument.hpp"
#include "../Lua/shady-lua.hpp"
#include "reader.hpp"

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
    DWORD attr = GetFileAttributes(pathu8.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return -1;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
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
        id = package.appendPackage(pathu8.c_str());
    } else {
        const char* filename = pathu8.c_str();
        id = package.appendFile(PathFindFileName(filename), filename);
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

void FileLoaderCallback(SokuData::FileLoaderData& data) {
	if (strcmp(PathFindExtension(data.fileName), ".lua") != 0)
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
        if (type.isEncrypted) {
            data.inputFormat = DataFormat::RAW;
            setup_entry_reader(data, *iter);
        } else if (type == ShadyCore::FileType::TYPE_UNKNOWN) {
            data.inputFormat = DataFormat::RAW;
            std::istream& input = iter->open();
            size_t size = iter->getSize();
            char* buffer = new char[size];
            input.read(buffer, size);
            iter->close();
            setup_buffer_reader(data, buffer, size);
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
}

void UnloadIntercept() {
    package.clear();

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

void* EnablePackage(const std::string& name, const std::string& ext, std::vector<FileID>& sokuIds) {
	std::string path(name + ext);
	if (PathIsRelative(path.c_str())) path = modulePath + "\\" + path;
    void* script = 0;

	if (iniConfig.useIntercept) {
		sokuIds.push_back(Soku::AddFile(s2ws(path)));
        if (ShadyLua::isAvailable() && package.findFile("init.lua") != package.end())
            script = ShadyLua::LoadFromGeneric(&package, _lua_open, _lua_read, "init.lua");
	} else if (ext == ".zip") {
		sokuIds.push_back(Soku::AddFile(s2ws(path)));
        if (ShadyLua::isAvailable() && ShadyLua::ZipExists(path.c_str(), "init.lua"))
            script = ShadyLua::LoadFromZip(path.c_str(), "init.lua");
	} else if (ext == ".dat") {
		char magicWord[6];
		std::ifstream input; input.open(path, std::ios::binary);
		input.unsetf(std::ios::skipws);
		input.read(magicWord, 6);
		input.close();
		if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
			sokuIds.push_back(Soku::AddFile(s2ws(path)));
		} else {
			reinterpret_cast<void(*)(const char *)>(0x0040D1D0u)(path.c_str());
		}
	} else {
		unsigned int attr = GetFileAttributes(path.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES) return 0;
		if (attr & FILE_ATTRIBUTE_DIRECTORY) {
			std::stack<std::string> dirStack;
			dirStack.push(path);
            std::string luaRoot(modulePath + "\\" + name + ext + "\\init.lua");

			while (!dirStack.empty()) {
				WIN32_FIND_DATA findData;
				HANDLE hFind;
				std::string current = dirStack.top(); dirStack.pop();
				if ((hFind = FindFirstFile((current + "\\*").c_str(), &findData)) != INVALID_HANDLE_VALUE) {
					do {
						if (findData.cFileName[0] == '.') continue;
                        std::string filename(current + "\\" + findData.cFileName);
						if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
							dirStack.push(filename);
						else {
                            sokuIds.push_back(Soku::AddFile(s2ws(filename)));
                            if (ShadyLua::isAvailable() && filename == luaRoot)
                                script = ShadyLua::LoadFromFilesystem(filename.c_str());
                        }
					} while(FindNextFile(hFind, &findData));
					FindClose(hFind);
				}
			}
		}
	}

    return script;
}

void DisablePackage(std::vector<FileID>& sokuIds, void* script) {
    if (script && ShadyLua::isAvailable()) ShadyLua::FreeScript(script);
    for(auto& id : sokuIds) {
        Soku::RemoveFile(id);
    } sokuIds.clear();
}