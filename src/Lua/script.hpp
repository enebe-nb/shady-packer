#pragma once

#include <lua.hpp>
#include <unordered_map>
#include <mutex>
#include <filesystem>

namespace ShadyLua {
    typedef void* (*fnOpen_t)(void* userdata, const char* filename);
    typedef size_t (*fnRead_t)(void* userdata, void* file, char* buffer, size_t size);
    typedef void (*fnDestroy_t)(void* userdata);

    class LuaScript {
    public:
        lua_State* L;
        std::mutex mutex;
    protected:
        void* userdata;
        fnOpen_t fnOpen;
        fnRead_t fnRead;
        fnDestroy_t fnDestroy;

    public:
        LuaScript(void* userdata, fnOpen_t open, fnRead_t read, fnDestroy_t destroy = nullptr);
        LuaScript(const LuaScript&) = delete;
        LuaScript(LuaScript&&) = delete;
        virtual ~LuaScript();

        virtual int load(const char* filename, const char* mode = 0);
        inline void* open(const char* filename)
            {return fnOpen(userdata, filename);}
        inline size_t read(void* file, char* buffer, size_t size)
            {return fnRead(userdata, file, buffer, size);}
        int run();
    };

    class LuaScriptFS : public LuaScript {
    private:
        std::filesystem::path root;

        static void* fnOpen(void* userdata, const char* filename);
        static size_t fnRead(void* userdata, void* file, char* buffer, size_t size);
    public:
        inline LuaScriptFS(const std::filesystem::path& root)
            : LuaScript(this, LuaScriptFS::fnOpen, LuaScriptFS::fnRead), root(root) {}
    };

/*
    class LuaScriptZip : public LuaScript {
    private:
        std::string zipFile;
        void* zipObject;
        int openCount = 0;

        static void* fnOpen(void* userdata, const char* filename);
        static size_t fnRead(void* userdata, void* file, char* buffer, size_t size);
    public:
        inline LuaScriptZip(const std::string& zipFile)
            : LuaScript(this, fnOpen, fnRead), zipFile(zipFile) {}
    };
*/
    extern std::unordered_map<lua_State*, ShadyLua::LuaScript*> ScriptMap;
}
