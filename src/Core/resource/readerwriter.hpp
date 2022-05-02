#pragma once
#include "../resource.hpp"
#include "../baseentry.hpp"
#include <istream>

namespace ShadyCore {
	class FileType {
	public:
		enum Type : short {
			TYPE_UNKNOWN, TYPE_TEXT, TYPE_TABLE,
			TYPE_LABEL, TYPE_IMAGE, TYPE_PALETTE,
			TYPE_SFX, TYPE_BGM, TYPE_SCHEMA,
			TYPE_TEXTURE,
		};

		enum Format : short {
			FORMAT_UNKNOWN,

			TEXT_GAME, TEXT_NORMAL,
			TABLE_GAME, TABLE_CSV,
			LABEL_RIFF, LABEL_LBL,
			IMAGE_GAME, IMAGE_PNG, IMAGE_BMP,
			PALETTE_PAL, PALETTE_ACT,
			SFX_GAME, SFX_WAV,
			BGM_OGG,
			SCHEMA_XML, SCHEMA_GAME_GUI, SCHEMA_GAME_ANIM, SCHEMA_GAME_PATTERN,
			TEXTURE_DDS,
		};

		// --- DATA ---
		Type type;
		Format format;
		uint32_t extValue;

		FileType(const FileType&) = default;
		constexpr FileType(Type type, Format format, uint32_t extValue = 0)
			: type(type), format(format), extValue(extValue) {}
		explicit constexpr FileType(uint32_t extValue = 0)
			: type(TYPE_UNKNOWN), format(FORMAT_UNKNOWN), extValue(extValue) {}

		static constexpr uint32_t getExtValue(const char* ext) {
			uint32_t value = 0;
			if (ext) for (int i = 0; i < 4 && *ext; ++i) value += *ext++ << (3 - i) * 8;
			return value;
		}

		template<class T>
		static inline T& appendExtValue(T& name, uint32_t ext) {
			for (int i = 0; i < 4 && ext;) {
				name += ext >> (3 - i) * 8;
				ext &= 0xffffffffu >> ++i * 8;
			} return name;
		}
		template<class T> inline T& appendExtValue(T& name) const { return appendExtValue(name, extValue); }

		static FileType get(uint32_t extValue);
		static inline FileType get(const char* name) {
			while(*name != '\0' && *name != '.') ++name;
			return get(getExtValue(name));
		}

		inline bool operator==(Type t) const { return type == t; }
		inline bool operator!=(Type t) const { return type != t; }
	};

	typedef void (*ResourceReader_t)(Resource*, std::istream&);
	typedef void (*ResourceWriter_t)(Resource*, std::ostream&);
	ResourceReader_t getResourceReader(const ShadyCore::FileType&);
	ResourceWriter_t getResourceWriter(const ShadyCore::FileType&);

	Resource* createResource(const FileType::Type);
	void destroyResource(const FileType::Type, Resource*);
	void convertResource(const FileType::Type, const FileType::Format, std::istream&, const FileType::Format, std::ostream&);
}
