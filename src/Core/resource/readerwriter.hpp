#pragma once
#include "../resource.hpp"
#include "../baseentry.hpp"

namespace ShadyCore {
    class FileType {
    public:
        const enum Type {
            TYPE_UNKNOWN, TYPE_PACKAGE, TYPE_TEXT, TYPE_LABEL,
			TYPE_IMAGE, TYPE_PALETTE, TYPE_SFX, TYPE_GUI,
			TYPE_ANIMATION, TYPE_PATTERN,
        } type;
        const bool isEncrypted;
        const uint32_t extValue;
        const char* normalExt;
        const char* inverseExt;

        static const FileType& getSimple(const char*);
        static const FileType& get(const char*, std::istream&);
        static const FileType& get(BasePackageEntry& entry);
		static const FileType& getByName(const char* name);

        inline bool operator==(Type t) const { return type == t; }
		inline bool operator!=(Type t) const { return type != t; }
    private:
        constexpr FileType(Type type, bool isEncrypted, const char* normalExt = 0, const char* inverseExt = 0);

		static const FileType typeUnknown;
		static const FileType typePackage;
		static const FileType typeDText;
		static const FileType typeDTable;
		static const FileType typeDLabel;
		static const FileType typeDImage;
		static const FileType typeDPalette;
		static const FileType typeDSfx;
		static const FileType typeDGui;
        static const FileType typeDGuiDEPRECATED;  // TODO DEPRECATED
        static const FileType typeDAnimation;
		static const FileType typeDPattern;
		static const FileType typeEText;
		static const FileType typeETable;
		static const FileType typeELabel;
		static const FileType typeEImage;
		static const FileType typeEPalette;
		static const FileType typeESfx;
		static const FileType typeEGui;
		static const FileType typeEAnimation;
		static const FileType typeEPattern;

	};

	Resource* createResource(const FileType&);
	//Resource* readResource(BasePackageEntry& entry);
	Resource* readResource(const FileType&, std::istream&);
    void readResource(Resource*, std::istream&, bool);
	void writeResource(Resource*, std::ostream&, bool);
	void convertResource(const FileType&, std::istream&, std::ostream&);

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
