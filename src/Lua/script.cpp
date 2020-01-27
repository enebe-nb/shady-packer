#include "script.hpp"
#include "logger.hpp"

#include <shlwapi.h>
#include <fstream>
#include <unordered_map>
#include <zip.h>

namespace {
    std::unordered_map<lua_State*, ShadyLua::LuaScript*> scriptMap;

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
	if (lua_pcall(L, 0, LUA_MULTRET, 0)) {
		Logger::Error(lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
	return true;
}

//lua_Debug info;
//lua_getstack(state, 1, &info);
//lua_getinfo(state, "Sl", &info);
//char lineNumber[33]; itoa(info.currentline, lineNumber, 10);
//Logger::Debug(info.source, ":", lineNumber);
static int _lua_print(lua_State* L) {
    std::string line;
    int nargs = lua_gettop(L);
    for (int i = 1; i <= nargs; ++i) {
        if (i != 1) line += " ";
        line += lua_tostring(L, i);
    }

    Logger::Debug(line);
    return 0;
}

static int _lua_loadfile(lua_State* L) {
    ShadyLua::LuaScript* script = scriptMap.at(L);
    const char *filename = lua_tostring(L, 1);
    const char *mode = luaL_optstring(L, 2, NULL);
    int env = (!lua_isnone(L, 3) ? 3 : 0);

    if (script->load(filename, mode)) {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    } else if (env) {
        lua_pushvalue(L, env);
        if (!lua_setupvalue(L, -2, 1)) lua_pop(L, 1);
    } return 1;
}

static int _lua_dofile(lua_State* L) {
    ShadyLua::LuaScript* script = scriptMap.at(L);
    const char *filename = lua_tostring(L, 1);
    lua_settop(L, 1);
    if (script->load(filename)) return lua_error(L);
    lua_call(L, 0, LUA_MULTRET, 0);
    return lua_gettop(L) - 1;
}

ShadyLua::LuaScript::LuaScript(void* userdata, fnOpen_t open, fnRead_t read)
    : L(luaL_newstate()), userdata(userdata), open(open), read(read) {
    scriptMap[L] = this;
    luaL_openlibs(L);
    lua_register(L, "print", _lua_print);
    lua_register(L, "loadfile", _lua_loadfile);
    lua_register(L, "dofile", _lua_dofile);
}

ShadyLua::LuaScript::~LuaScript() {
    scriptMap.erase(L);
    lua_close(L);
}

int ShadyLua::LuaScript::load(const char* filename, const char* mode) {
    Reader reader;
    reader.fnRead = read;
    reader.userdata = userdata;
    reader.file = open(userdata, filename);
    int result = lua_load(L, Reader::read, &reader, filename, mode);
    if (result != LUA_OK) Logger::Error(lua_tostring(L, 1));
    return result;
}

void* ShadyLua::LuaScriptFS::fnOpen(void* userdata, const char* filename) {
    LuaScriptFS* script = reinterpret_cast<LuaScriptFS*>(userdata);
    char filepath[MAX_PATH];
    if (!PathCanonicalize(filepath, filename)) {
        Logger::Error("Invalid path: ", filename);
        return 0;
    } if (!PathIsRelative(filepath) || !filepath[0]) {
        Logger::Error("Can't load path: ", filename);
        return 0;
    } if (!PathFileExists((script->root + filepath).c_str())) {
        Logger::Error("File \"", filename, "\" doesn't exist.");
        return 0;
    }

    std::ifstream* input = new std::ifstream(script->root + filepath);
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

/*
LuaScript* LuaScript::fromFilesystem(const char* path) {
    const char* filename = PathFindFileName(path);
    LuaScriptFS* script = new LuaScriptFS(std::string(path, filename));
    if (script->load(filename) != LUA_OK) {
        Logger::Error(script->lastError);
        delete script;
        return 0;
    } return script;
}

LuaScript* LuaScript::fromSoku(const char* path) {
    const char* filename = PathFindFileName(path);
    LuaScriptSoku* script = new LuaScriptSoku();
    Logger::Debug("???");
    if (script->load(filename) != LUA_OK) {
        Logger::Error(script->lastError);
        delete script;
        return 0;
    } return 0;
}
*/
