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
		};

		enum Format : short {
			FORMAT_UNKNOWN      = -1,

			TEXT_GAME           = 0,
			TEXT_NORMAL         = 1,
			TABLE_GAME          = 0,
			TABLE_CSV           = 1,
			LABEL_RIFF          = 0,
			LABEL_LBL           = 1,
			IMAGE_GAME          = 0,
			IMAGE_PNG           = 1,
			PALETTE_PAL         = 0,
			PALETTE_ACT         = 1,
			SFX_GAME            = 0,
			SFX_WAV             = 1,
			BGM_OGG             = 0,

			SCHEMA_GAME_GUI     = 0,
			SCHEMA_GAME_ANIM    = 4,
			SCHEMA_GAME_PATTERN = 5,
			SCHEMA_XML_GUI      = 1,
			SCHEMA_XML_ANIM     = 2,
			SCHEMA_XML_PATTERN  = 3,
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

	Resource* createResource(const FileType::Type, const FileType::Format); // TODO should receive only Type
	Resource* readResource(const FileType::Type, const FileType::Format, std::istream&);
	void readResource(Resource*, const FileType::Format, std::istream&);
	void writeResource(Resource*, const FileType::Format, std::ostream&);
	void convertResource(const FileType::Type, const FileType::Format, std::istream&, const FileType::Format, std::ostream&);

	class ResourceDReader : public ResourceVisitor {
	private:
		std::istream& input;
	public:
		inline ResourceDReader(Resource* resource, std::istream& input) : input(input) { resource->visit(this); }

		void accept(TextResource &) override;
		void accept(LabelResource &) override;
		void accept(Image &) override;
		void accept(Palette &) override;
		void accept(Sfx &) override;
		void accept(GuiRoot &) override;
		void accept(GuiImage &) override;
		void accept(GuiView &) override;
		void accept(GuiView::Static &) override;
		void accept(GuiView::Mutable &) override;
		void accept(GuiView::Number &) override;
		void accept(Pattern &) override;
		void accept(Sequence &) override;
		void accept(Sequence::Clone &) override;
		void accept(Sequence::Animation &) override;
		void accept(Sequence::Move &) override;
		void accept(Frame &) override;
		void accept(Frame::Animation &) override;
		void accept(Frame::Move &) override;
		void accept(BlendOptions &) override;
		void accept(MoveTraits &) override;
		void accept(BBoxList &) override;
		void accept(MoveEffect &) override;
	};

	class ResourceEReader : public ResourceVisitor {
	private:
		std::istream& input;
	public:
		inline ResourceEReader(Resource* resource, std::istream& input) : input(input) { resource->visit(this); }

		void accept(TextResource &) override;
		void accept(LabelResource &) override;
		void accept(Image &) override;
		void accept(Palette &) override;
		void accept(Sfx &) override;
		void accept(GuiRoot &) override;
		void accept(GuiImage &) override;
		void accept(GuiView &) override;
		void accept(GuiView::Static &) override;
		void accept(GuiView::Mutable &) override;
		void accept(GuiView::Number &) override;
		void accept(Pattern &) override;
		void accept(Sequence &) override;
		void accept(Sequence::Clone &) override;
		void accept(Sequence::Animation &) override;
		void accept(Sequence::Move &) override;
		void accept(Frame &) override;
		void accept(Frame::Animation &) override;
		void accept(Frame::Move &) override;
		void accept(BlendOptions &) override;
		void accept(MoveTraits &) override;
		void accept(BBoxList &) override;
		void accept(MoveEffect &) override;
	};

	class ResourceDWriter : public ResourceVisitor {
	private:
		std::ostream& output;
	public:
		inline ResourceDWriter(Resource* resource, std::ostream& output) : output(output) { resource->visit(this); }

		void accept(TextResource &) override;
		void accept(LabelResource &) override;
		void accept(Image &) override;
		void accept(Palette &) override;
		void accept(Sfx &) override;
		void accept(GuiRoot &) override;
		void accept(GuiImage &) override;
		void accept(GuiView &) override;
		void accept(GuiView::Static &) override;
		void accept(GuiView::Mutable &) override;
		void accept(GuiView::Number &) override;
		void accept(Pattern &) override;
		void accept(Sequence &) override;
		void accept(Sequence::Clone &) override;
		void accept(Sequence::Animation &) override;
		void accept(Sequence::Move &) override;
		void accept(Frame &) override;
		void accept(Frame::Animation &) override;
		void accept(Frame::Move &) override;
		void accept(BlendOptions &) override;
		void accept(MoveTraits &) override;
		void accept(BBoxList &) override;
		void accept(MoveEffect &) override;
	};

	class ResourceEWriter : public ResourceVisitor {
	private:
		std::ostream& output;
	public:
		inline ResourceEWriter(Resource* resource, std::ostream& output) : output(output) { resource->visit(this); }

		void accept(TextResource &) override;
		void accept(LabelResource &) override;
		void accept(Image &) override;
		void accept(Palette &) override;
		void accept(Sfx &) override;
		void accept(GuiRoot &) override;
		void accept(GuiImage &) override;
		void accept(GuiView &) override;
		void accept(GuiView::Static &) override;
		void accept(GuiView::Mutable &) override;
		void accept(GuiView::Number &) override;
		void accept(Pattern &) override;
		void accept(Sequence &) override;
		void accept(Sequence::Clone &) override;
		void accept(Sequence::Animation &) override;
		void accept(Sequence::Move &) override;
		void accept(Frame &) override;
		void accept(Frame::Animation &) override;
		void accept(Frame::Move &) override;
		void accept(BlendOptions &) override;
		void accept(MoveTraits &) override;
		void accept(BBoxList &) override;
		void accept(MoveEffect &) override;
	};
}
