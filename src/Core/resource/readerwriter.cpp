#include "readerwriter.hpp"
#include <string>
#include <unordered_set>

// const ShadyCore::FileType ShadyCore::FileType::typeDText(TYPE_TEXT, false, ".txt", ".cv0");
// const ShadyCore::FileType ShadyCore::FileType::typeDTable(TYPE_TEXT, false, ".csv", ".cv1");
// const ShadyCore::FileType ShadyCore::FileType::typeDLabel(TYPE_LABEL, false, ".lbl", ".sfl");
// const ShadyCore::FileType ShadyCore::FileType::typeDImage(TYPE_IMAGE, false, ".png", ".cv2");
// const ShadyCore::FileType ShadyCore::FileType::typeDPalette(TYPE_PALETTE, false, ".act", ".pal");
// const ShadyCore::FileType ShadyCore::FileType::typeDSfx(TYPE_SFX, false, ".wav", ".cv3");
// const ShadyCore::FileType ShadyCore::FileType::typeDGui(TYPE_GUI, false, ".xml", ".dat");
// const ShadyCore::FileType ShadyCore::FileType::typeDAnimation(TYPE_ANIMATION, false, ".xml", ".pat");
// const ShadyCore::FileType ShadyCore::FileType::typeDPattern(TYPE_PATTERN, false, ".xml", ".pat");
// const ShadyCore::FileType ShadyCore::FileType::typeEText(TYPE_TEXT, true, ".cv0", ".txt");
// const ShadyCore::FileType ShadyCore::FileType::typeETable(TYPE_TEXT, true, ".cv1", ".csv");
// const ShadyCore::FileType ShadyCore::FileType::typeELabel(TYPE_LABEL, true, ".sfl", ".lbl");
// const ShadyCore::FileType ShadyCore::FileType::typeEImage(TYPE_IMAGE, true, ".cv2", ".png");
// const ShadyCore::FileType ShadyCore::FileType::typeEPalette(TYPE_PALETTE, true, ".pal", ".act");
// const ShadyCore::FileType ShadyCore::FileType::typeESfx(TYPE_SFX, true, ".cv3", ".wav");
// const ShadyCore::FileType ShadyCore::FileType::typeEGui(TYPE_GUI, true, ".dat", ".xml");
// const ShadyCore::FileType ShadyCore::FileType::typeEAnimation(TYPE_ANIMATION, true, ".pat", ".xml");
// const ShadyCore::FileType ShadyCore::FileType::typeEPattern(TYPE_PATTERN, true, ".pat", ".xml");
namespace {
    using FT = ShadyCore::FileType;

    struct _comparator {
        using is_transparent = uint32_t;
        inline std::size_t operator()(const FT& v) const noexcept { return std::hash<uint32_t>()(v.extValue); }
        inline std::size_t operator()(const uint32_t v) const noexcept { return std::hash<uint32_t>()(v); }
        inline bool operator()(const FT& l, const FT& r) const noexcept { return l.extValue == r.extValue; }
        inline bool operator()(const uint32_t l, const FT& r) const noexcept { return l == r.extValue; }
    };

    const std::unordered_set<FT, _comparator, _comparator> typeMap = {
        FT(FT::TYPE_TEXT, FT::TEXT_NORMAL, FT::getExtValue(".txt")),
        FT(FT::TYPE_TEXT, FT::FORMAT_UNKNOWN, FT::getExtValue(".cv0")),
        FT(FT::TYPE_TABLE, FT::TABLE_CSV, FT::getExtValue(".csv")),
        FT(FT::TYPE_TABLE, FT::FORMAT_UNKNOWN, FT::getExtValue(".cv1")),
        FT(FT::TYPE_LABEL, FT::LABEL_LBL, FT::getExtValue(".lbl")),
        FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl")),
        FT(FT::TYPE_IMAGE, FT::IMAGE_PNG, FT::getExtValue(".png")),
        FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2")),
        FT(FT::TYPE_PALETTE, FT::PALETTE_ACT, FT::getExtValue(".act")),
        FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal")),
        FT(FT::TYPE_SFX, FT::SFX_WAV, FT::getExtValue(".wav")),
        FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3")),

        FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN, FT::getExtValue(".xml")),
        FT(FT::TYPE_SCHEMA, FT::SCHEMA_GAME_GUI, FT::getExtValue(".dat")),
        FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN, FT::getExtValue(".pat")),
    };
}

ShadyCore::FileType ShadyCore::FileType::get(uint32_t extValue) {
    auto i = typeMap.find(extValue);
    if (i == typeMap.end()) return FileType(extValue);
    return *i;
}

/*
const ShadyCore::FileType& ShadyCore::FileType::getSimple(const char* name) {
    static const FileType* extMap[] = {
        &typeDPalette, // ".act"
        &typeDTable, // ".csv"
        &typeEText, // ".cv0"
        &typeETable, // ".cv1"
        &typeEImage, // ".cv2"
        &typeESfx, // ".cv3"
        &typeEGui, // ".dat"
        &typeDLabel, // ".lbl"
        &typeEPalette, // ".pal"
        &typeEPattern, // ".pat"
        &typeDImage, // ".png"
        &typeELabel, // ".sfl"
        &typeDText, // ".txt"
        &typeDSfx, // ".wav"
        &typeDPattern, // ".xml"
    }; const uint32_t extMapCount = 15;

    const char* ext = strrchr(name, '.');
    if (!ext) return typePackage;
    if (strcmp(ext, ".zip") == 0) return typePackage;

    uint32_t extV = getExtValue(ext);
    uint32_t lb = 0;
    uint32_t ub = extMapCount - 1;
    const FileType* type = &typeUnknown;

    while(ub >= lb) {
        uint32_t i = lb + (ub - lb) / 2;
        if (extV == extMap[i]->extValue) { type = extMap[i]; break; }
        if (extV > extMap[i]->extValue) lb = i + 1;
        else ub = i - 1;
    }

    return *type;
}

const ShadyCore::FileType& ShadyCore::FileType::get(const char* name, std::istream& input) {
    const FileType& type = getSimple(name);

    if (type.isEncrypted && type == TYPE_GUI) {
        uint32_t version; input.read((char*)&version, 4);
        input.seekg(0);
        if (version != 4) return typePackage;
    } else if (type.isEncrypted && type == TYPE_PATTERN) {
        uint8_t version; input.read((char*)&version, 1);
        input.seekg(0);
        if (version != 5) return typeUnknown;
        size_t len = strlen(name);
        if (len >= 10 && strcmp(name + len - 10, "effect.pat") == 0
            || len >= 9 && strcmp(name + len - 9, "stand.pat") == 0)
            return typeEAnimation;
    } else if (!type.isEncrypted && type == TYPE_PATTERN) {
        char buffer[128];
        while (input.get(buffer[0]) && input.gcount()) {
            if (buffer[0] == '<') {
                int i = 0;
                for (input.get(buffer[0]); buffer[i] && !strchr(" />", buffer[i]); input.get(buffer[++i]));
                buffer[i] = '\0';
				if (strcmp(buffer, "movepattern") == 0) { input.seekg(0); return typeDPattern; }
                if (strcmp(buffer, "animpattern") == 0) { input.seekg(0); return typeDAnimation; }
				if (strcmp(buffer, "layout") == 0) { input.seekg(0); return typeDGui; }
                if (!strchr("?", buffer[0])) break;
            }
        }
		input.seekg(0);
		return typeUnknown;
    }

    return type;
}

const ShadyCore::FileType& ShadyCore::FileType::get(BasePackageEntry& entry) {
	const FileType& type = getSimple(entry.getName());

	if (type.isEncrypted && type == TYPE_GUI) {
		std::istream& input = entry.open();
		uint32_t version; input.read((char*)&version, 4);
		entry.close();
		if (version != 4) return typePackage;
	} else if (type.isEncrypted && type == TYPE_PATTERN) {
		std::istream& input = entry.open();
		uint8_t version; input.read((char*)&version, 1);
		entry.close();
		if (version != 5) return typeUnknown;
        size_t len = strlen(entry.getName());
        if (len >= 10 && strcmp(entry.getName() + len - 10, "effect.pat") == 0
            || len >= 9 && strcmp(entry.getName() + len - 9, "stand.pat") == 0)
            return typeEAnimation;
    } else if (!type.isEncrypted && type == TYPE_PATTERN) {
        std::istream& input = entry.open();
        char buffer[128];
        while (input.get(buffer[0]) && input.gcount()) {
            if (buffer[0] == '<') {
                int i = 0;
                for (input.get(buffer[0]); buffer[i] && !strchr(" />", buffer[i]); input.get(buffer[++i]));
                buffer[i] = '\0';
				if (strcmp(buffer, "movepattern") == 0) { entry.close(); return typeDPattern; }
				if (strcmp(buffer, "animpattern") == 0) { entry.close(); return typeDAnimation; }
				if (strcmp(buffer, "layout") == 0) { entry.close(); return typeDGui; }
                if (!strchr("?", buffer[0])) break;
            }
        }
		entry.close();
		return typeUnknown;
	}

	return type;
}

const ShadyCore::FileType& ShadyCore::FileType::getByName(const char* name) {
    if (!name) return typeUnknown;
    // TODO remaining
    if (strcmp(name, "lbl") == 0
        || strcmp(name, "label") == 0) return typeDLabel;
    if (strcmp(name, "png") == 0) return typeDImage;
    if (strcmp(name, "act") == 0) return typeDPalette;
    if (strcmp(name, "wav") == 0) return typeDSfx;
    return typeUnknown;
}
*/

ShadyCore::Resource* ShadyCore::createResource(const FileType::Type type, const FileType::Format format) {
    switch(type) {
	case FileType::TYPE_TEXT: return new TextResource();
    case FileType::TYPE_TABLE: return new TextResource();
	case FileType::TYPE_LABEL: return new LabelResource();
	case FileType::TYPE_PALETTE: return new Palette();
    case FileType::TYPE_IMAGE: return new Image();
	case FileType::TYPE_SFX: return new Sfx();
    case FileType::TYPE_SCHEMA:
        // TODO simplify schema selection
        if (format == FileType::SCHEMA_GAME_GUI
            || format == FileType::SCHEMA_XML_GUI) return new GuiRoot();
        if (format == FileType::SCHEMA_GAME_ANIM
            || format == FileType::SCHEMA_XML_ANIM) return new Pattern(true);
        if (format == FileType::SCHEMA_GAME_PATTERN
            || format == FileType::SCHEMA_XML_PATTERN) return new Pattern(false);
        throw std::runtime_error("Unknown Format ID: " + std::to_string(format));
	default: return 0;
    }
}

ShadyCore::Resource* ShadyCore::readResource(const FileType::Type type, const FileType::Format format, std::istream& input) {
	Resource* resource = createResource(type, format);
	if (resource) {
        // TODO remake readers for any format
		if (format == 0 || format >= 4) ResourceEReader(resource, input);
		else ResourceDReader(resource, input);
	} return resource;
}

	//void readResource(Resource*, const FileType::Format, std::istream&);
	//void writeResource(Resource*, const FileType::Format, std::ostream&);
	//void convertResource(const FileType::Type, const FileType::Format, std::istream&, const FileType::Format, std::ostream&);

void ShadyCore::readResource(Resource* resource, const FileType::Format format, std::istream& input) {
    // TODO remake readers for any format
	if (format == 0 || format >= 4) ResourceEReader(resource, input);
    else ResourceDReader(resource, input);
}

void ShadyCore::writeResource(Resource* resource, const FileType::Format format, std::ostream& output) {
    // TODO remake readers for any format
	if (format == 0 || format >= 4) ResourceEWriter(resource, output);
	else ResourceDWriter(resource, output);
}

void ShadyCore::convertResource(const FileType::Type type, const FileType::Format inputFormat, std::istream& input, const FileType::Format outputFormat, std::ostream& output) {
    if (inputFormat == outputFormat) { output << input.rdbuf(); return; }

	Resource* resource = createResource(type, inputFormat);
	if (!resource) throw; // TODO Error Handling
    // TODO remake readers for any format
	if (inputFormat == 0 || inputFormat >= 4) {
		ResourceEReader(resource, input);
		ResourceDWriter(resource, output);
	} else {
		ResourceDReader(resource, input);
		ResourceEWriter(resource, output);
	} delete resource;
}

void ShadyCore::TextResource::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::LabelResource::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Image::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Palette::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Sfx::visit(ResourceVisitor* visitor) { visitor->accept(*this); }

void ShadyCore::GuiRoot::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::GuiImage::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::GuiView::visit(ResourceVisitor* visitor) { visitor->accept(*this); impl->visit(visitor); }
void ShadyCore::GuiView::Static::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::GuiView::Mutable::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::GuiView::Number::visit(ResourceVisitor* visitor) { visitor->accept(*this); }

void ShadyCore::Pattern::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Sequence::visit(ResourceVisitor* visitor) { visitor->accept(*this); if(impl) impl->visit(visitor); }
void ShadyCore::Sequence::Clone::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Sequence::Animation::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Sequence::Move::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Frame::visit(ResourceVisitor* visitor) { visitor->accept(*this); impl->visit(visitor); }
void ShadyCore::Frame::Animation::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::Frame::Move::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::BlendOptions::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::MoveTraits::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::BBoxList::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
void ShadyCore::MoveEffect::visit(ResourceVisitor* visitor) { visitor->accept(*this); }
