#include "script.hpp"
#include "logger.hpp"
#include "lualibs.hpp"
#include "lualibs/soku.hpp"

#include <fstream>
#include <unordered_map>

std::unordered_map<lua_State*, ShadyLua::LuaScript*> ShadyLua::ScriptMap;

namespace {
    struct Reader {
        ShadyLua::LuaScript::File* file;
        char buffer[4096];
        static const char* read(lua_State* L, Reader* reader, size_t* size) {
            reader->file->input.read(reader->buffer, 4096);
            *size = reader->file->input.gcount();
            return reader->buffer;
        }
    };

    struct FileFS : ShadyLua::LuaScript::File {
        std::ifstream data;
        inline FileFS(const std::filesystem::path& filename) : data(filename), File(data) {}
    };
}

int ShadyLua::LuaScript::run() {
    std::lock_guard guard(mutex);
	if (lua_pcall(L, 0, 0, 0)) {
		Logger::Error(lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
	return true;
}

ShadyLua::LuaScript::LuaScript(void* userdata, fnOpen_t open, fnClose_t close, fnDestroy_t destroy)
    : L(luaL_newstate()), userdata(userdata), fnOpen(open), fnClose(close), fnDestroy(destroy) {
    ScriptMap[L] = this;
    LualibBase(L);
    LualibMemory(L);
    LualibResource(L);
    LualibSoku(L);
    LualibGui(L);
    LualibBattle(L);
}

ShadyLua::LuaScript::~LuaScript() {
    std::lock_guard guard(mutex);
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
    reader.file = this->openFile(filename);
    if (!reader.file) { lua_pushstring(L, "Cannot open file."); return LUA_ERRRUN; }
    int result = lua_load(L, (lua_Reader)Reader::read, &reader, filename, mode);
    this->closeFile(reader.file);
    if (result != LUA_OK) Logger::Error(lua_tostring(L, 1));
    return result;
}

ShadyLua::LuaScriptFS::LuaScriptFS(const std::filesystem::path& basePath)
    : LuaScript(this, LuaScriptFS::fnOpen, LuaScriptFS::fnClose), basePath(basePath), package(basePath) {
    LualibLoader(L, &package);
}

ShadyLua::LuaScriptFS::File* ShadyLua::LuaScriptFS::fnOpen(void* userdata, const char* filename) {
    LuaScriptFS* script = reinterpret_cast<LuaScriptFS*>(userdata);
    std::filesystem::path path(script->basePath / std::filesystem::u8path(filename)); // TODO check utf

    FileFS* file = new FileFS(path);
    if (file->input.fail()) {
        Logger::Error("Can't open file: ", filename);
        delete file;
        return 0;
    } return file;
}

void ShadyLua::LuaScriptFS::fnClose(void* userdata, File* file) {
    delete reinterpret_cast<FileFS*>(file);
}
