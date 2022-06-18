#pragma once

#include <string>
#include "../logger.hpp"
#include "../../Core/resource.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include <LuaBridge/RefCountedObject.h>
#include <SokuLib.hpp>

namespace ShadyLua {
    // TODO arguments on Emitters
    // TODO remake hooks
    void EmitSokuEventRender();
    // void EmitSokuEventBattleEvent(int eventId);
    // void EmitSokuEventGameEvent(int sceneId);
    // void EmitSokuEventStageSelect(int* stageId);
    void EmitSokuEventFileLoader(const char* filename, std::istream **input, int*size);
    void EmitSokuEventFileLoader2(const char* filename, std::istream **input, int*size);
    void EmitSokuEventPlayerInfo(const SokuLib::PlayerInfo& info);

    inline void DefaultRenderHook() {
        Logger::Render();
        EmitSokuEventRender();
    }

    void LoadTamper();
    void UnloadTamper();
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