#pragma once

#include <string>
#include "../logger.hpp"
#include "../../Core/resource.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include <LuaBridge/RefCountedObject.h>
#include <SokuLib.hpp>
#include <unordered_map>

namespace ShadyLua {
    struct Hook {
        virtual ~Hook() = default;
    }; extern std::unordered_map<DWORD, std::unique_ptr<Hook>> hooks;

    template<DWORD addr, auto replFn>
    struct CallHook : Hook {
        using typeFn = decltype(replFn);
        static typeFn origFn;
        static constexpr DWORD ADDR = addr;

        inline CallHook() {
            DWORD prot; VirtualProtect((LPVOID)addr, 5, PAGE_EXECUTE_WRITECOPY, &prot);
            origFn = SokuLib::TamperNearJmpOpr(addr, replFn);
            VirtualProtect((LPVOID)addr, 5, prot, &prot);
        }

        virtual ~CallHook() {
            DWORD prot; VirtualProtect((LPVOID)addr, 5, PAGE_EXECUTE_WRITECOPY, &prot);
            SokuLib::TamperNearJmpOpr(addr, origFn);
            VirtualProtect((LPVOID)addr, 5, prot, &prot);
        }
    };

    template<DWORD addr, auto replFn>
    struct AddressHook : Hook {
        using typeFn = decltype(replFn);
        static typeFn origFn;
        static constexpr DWORD ADDR = addr;

        inline AddressHook() {
            DWORD prot; VirtualProtect((LPVOID)addr, 4, PAGE_EXECUTE_WRITECOPY, &prot);
            origFn = SokuLib::TamperDword(addr, replFn);
            VirtualProtect((LPVOID)addr, 4, prot, &prot);
        }

        virtual ~AddressHook() {
            DWORD prot; VirtualProtect((LPVOID)addr, 4, PAGE_EXECUTE_WRITECOPY, &prot);
            SokuLib::TamperDword(addr, origFn);
            VirtualProtect((LPVOID)addr, 4, prot, &prot);
        }
    };

    template<typename hookClass>
    inline void initHook() {
        static_assert(std::is_base_of<ShadyLua::Hook, hookClass>::value, "hookType not derived from ShadyLua::Hook");
        if (!ShadyLua::hooks[hookClass::ADDR]) ShadyLua::hooks[hookClass::ADDR].reset(new hookClass());
    }

    class LuaScript;
    void RemoveEvents(LuaScript*);
    void RemoveLoaderEvents(LuaScript*);

	class ResourceProxy : public luabridge::RefCountedObject {
	public:
		ShadyCore::Resource* const resource;
		const ShadyCore::FileType::Type type;
		const bool isOwner;

		inline ResourceProxy(ShadyCore::Resource* resource, ShadyCore::FileType::Type type, bool isOwner = true)
			: resource(resource), type(type), isOwner(isOwner) {}
		inline ResourceProxy(ShadyCore::FileType::Type type)
			: resource(ShadyCore::createResource(type)), type(type), isOwner(true) {}
		inline ~ResourceProxy() { if(isOwner) ShadyCore::destroyResource(type, resource); }
	};
}