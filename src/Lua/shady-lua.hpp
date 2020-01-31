#pragma once
#include <windows.h>
#include <SokuLib.h>

// Per dynamic module Singleton implementation with multithread protection
template<typename T>
class __Singleton{
protected:
    __Singleton() = default;
    __Singleton(const __Singleton&) = delete;
    __Singleton(__Singleton&&) = delete;
private:
    static T& instance() { static T t; return t;}
public:
    static T& get() {
        static std::once_flag flag;
        std::call_once(flag, instance);
        return instance();
    }
};

namespace ShadyLua {
    typedef void* (*fnOpen_t)(void* userdata, const char* filename);
    typedef size_t (*fnRead_t)(void* userdata, void* file, char* buffer, size_t size);

    class Manager : public __Singleton<Manager> {
    public:
        Soku::Module module;
        inline bool isAvailable() {return !module.Name().empty();}

        template <typename ReturnType, typename ...ArgTypes>
        inline ReturnType ProcCall(void*& procAddr, const char* procName, ArgTypes... args) {
            if (!module.IsInjected()) module.Inject();
            if (!procAddr) procAddr = GetProcAddress(GetModuleHandleW(module.Name().c_str()), procName);
            return reinterpret_cast<ReturnType(*)(ArgTypes...)>(procAddr)(args...);
        }

    private:
        static inline Soku::Module& findModule(const std::wstring& name) {
            for (auto& m : Soku::GetModuleList()) {
                if (m.Name() == name) return m;
            } return Soku::Module(-1);
        }

        Manager() : module(findModule(L"shady-lua")) {}
        friend __Singleton;
    };

    /*** Checks if Lua module can be used */
    inline bool isAvailable() {
        return Manager::get().isAvailable();
    }

    void* _LoadFromGeneric = 0;
    /*** Load lua scripts from filesystem and return it */
    inline void* LoadFromGeneric(void* userdata, fnOpen_t open, fnRead_t read, const char* filename) {
        return Manager::get().ProcCall<void*>
            (_LoadFromGeneric, "LoadFromGeneric", userdata, open, read, filename);
    }

    void* _LoadFromFilesystem = 0;
    /*** Load lua scripts from filesystem and return it */
    inline void* LoadFromFilesystem(const char* filename) {
        return Manager::get().ProcCall<void*>
            (_LoadFromFilesystem, "LoadFromFilesystem", filename);
    }

    void* _LoadFromZip = 0;
    /*** Load lua scripts from Soku Added Files and return it */
    inline void* LoadFromZip(const char* zipFile, const char* filename) {
        return Manager::get().ProcCall<void*>
            (_LoadFromZip, "LoadFromZip", zipFile, filename);
    }

    void* _FreeScript = 0;
    /*** Frees a loaded lua script */
    inline void FreeScript(void* script) {
        return Manager::get().ProcCall<void>
            (_FreeScript, "FreeScript", script);
    }

    void* _ZipExists = 0;
    /*** Checks if a file exists in a zip archive */
    inline bool ZipExists(const char* zipFile, const char* filename) {
        return Manager::get().ProcCall<bool>
            (_ZipExists, "ZipExists", zipFile, filename);
    }
}