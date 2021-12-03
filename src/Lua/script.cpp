#include "script.hpp"
#include "logger.hpp"
#include "lualibs.hpp"
#include "lualibs/soku.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <zip.h>

std::unordered_map<lua_State*, ShadyLua::LuaScript*> ShadyLua::ScriptMap;

namespace {
    struct Reader {
        ShadyLua::fnRead_t fnRead;
        void* userdata;
        void* file;
        char buffer[4096];
        static const char* read(lua_State* L, void* dt, size_t* size) {
            Reader* reader = reinterpret_cast<Reader*>(dt);
            *size = reader->fnRead(reader->userdata, reader->file, reader->buffer, 4096);
            return reader->buffer;
        }
    };
}

int ShadyLua::LuaScript::run() {
    std::lock_guard guard(mutex);
	if (lua_pcall(L, 0, LUA_MULTRET, 0)) {
		Logger::Error(lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
	return true;
}

ShadyLua::LuaScript::LuaScript(void* userdata, fnOpen_t open, fnRead_t read, fnDestroy_t destroy)
    : L(luaL_newstate()), userdata(userdata), fnOpen(open), fnRead(read), fnDestroy(destroy) {
    ScriptMap[L] = this;
}

ShadyLua::LuaScript::~LuaScript() {
    if (lua_getglobal(L, "AtExit") == LUA_TFUNCTION) {
        if (lua_pcall(L, 0, 0, 0)) Logger::Error(lua_tostring(L, -1));
    }
    RemoveEvents(this);
    ScriptMap.erase(L);
    if (fnDestroy) fnDestroy(userdata);
    lua_close(L);
}

int ShadyLua::LuaScript::load(const char* filename, const char* mode) {
    Reader reader;
    reader.fnRead = fnRead;
    reader.userdata = userdata;
    reader.file = fnOpen(userdata, filename);
    int result = lua_load(L, Reader::read, &reader, filename, mode);
    if (result != LUA_OK) Logger::Error(lua_tostring(L, 1));
    return result;
}

void* ShadyLua::LuaScriptFS::fnOpen(void* userdata, const char* filename) {
    LuaScriptFS* script = reinterpret_cast<LuaScriptFS*>(userdata);
    std::filesystem::path path(script->root / std::filesystem::u8path(filename));

    std::ifstream* input = new std::ifstream(path, std::ios::in | std::ios::binary);
    if (input->fail()) {
        Logger::Error("Can't open file: ", filename);
        delete input;
        return 0;
    } return input;
}

size_t ShadyLua::LuaScriptFS::fnRead(void* userdata, void* file, char* buffer, size_t size) {
    if (!file) return 0;
    std::istream* input = reinterpret_cast<std::istream*>(file);
    input->read(buffer, size);
    size = input->gcount();
    if (size == 0) delete input;
    return size;
}

/*
void* ShadyLua::LuaScriptZip::fnOpen(void* userdata, const char* filename) {
    LuaScriptZip* script = reinterpret_cast<LuaScriptZip*>(userdata);
    char filepath[MAX_PATH];
    if (!PathCanonicalize(filepath, filename)) {
        Logger::Error("Invalid path: ", filename);
        return 0;
    } if (!PathIsRelative(filepath) || !filepath[0]) {
        Logger::Error("Can't load path: ", filename);
        return 0;
    }

    if (!script->openCount++) {
        script->zipObject = zip_open(script->zipFile.c_str(), ZIP_RDONLY, 0);
        if (!script->zipObject) {
            Logger::Error("Can't open \"", script->zipFile, "\" archive");
            return 0;
        }
    }
    
    zip_int64_t index = zip_name_locate((zip_t*)script->zipObject, filename, ZIP_FL_NOCASE);
    if (index == -1) {
        Logger::Error("File \"", filename, "\" doesn't exist.");
        if (!--script->openCount) zip_close((zip_t*)script->zipObject);
        return 0;
    }

    zip_file_t* file = zip_fopen_index((zip_t*)script->zipObject, index, 0);
    if (!file) {
        Logger::Error("Can't open file: ", filename);
        return 0;
    }

    return file;
}

size_t ShadyLua::LuaScriptZip::fnRead(void* userdata, void* file, char* buffer, size_t size) {
    if (!file) return 0;
    size = zip_fread((zip_file_t*)file, buffer, size);
    if (size <= 0) {
        LuaScriptZip* script = reinterpret_cast<LuaScriptZip*>(userdata);
        zip_fclose((zip_file_t*)file);
        if (!--script->openCount) zip_close((zip_t*)script->zipObject);
        return 0;
    } return size;
}
*/