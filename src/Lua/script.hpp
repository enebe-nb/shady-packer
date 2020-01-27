#pragma once

#include <lua.hpp>
#include <string>

namespace ShadyLua {
    typedef void* (*fnOpen_t)(void* userdata, const char* filename);
    typedef size_t (*fnRead_t)(void* userdata, void* file, char* buffer, size_t size);

    class LuaScript {
    protected:
        lua_State* L;
        void* userdata;
        fnOpen_t open;
        fnRead_t read;

    public:
        LuaScript(void* userdata, fnOpen_t open, fnRead_t read);
        LuaScript(const LuaScript&) = delete;
        LuaScript(LuaScript&&) = delete;
        virtual ~LuaScript();

        int load(const char* filename, const char* mode = 0);
        int run();
    };

    class LuaScriptFS : public LuaScript {
    private:
        std::string root;

        static void* fnOpen(void* userdata, const char* filename);
        static size_t fnRead(void* userdata, void* file, char* buffer, size_t size);
    public:
        inline LuaScriptFS(const std::string& root)
            : LuaScript(this, fnOpen, fnRead), root(root) {}
    };

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
}
