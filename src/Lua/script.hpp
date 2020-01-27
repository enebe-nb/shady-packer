#pragma once

#include "types.hpp"
#include <lua.hpp>
#include <string>

namespace ShadyLua {
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

        //static LuaScript* fromFilesystem(const char* filename);
        //static LuaScript* fromSoku(const char* filename);
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
