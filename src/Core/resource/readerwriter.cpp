#include "readerwriter.hpp"
#include <cstring>

constexpr uint32_t getExtValue(const char* ext) {
    return ext ? (((uint32_t)ext[1]) << 16) + (((uint32_t)ext[2]) << 8) + ((uint32_t)ext[3]) : 0;
}

constexpr ShadyCore::FileType::FileType(Type type, bool isEncrypted, const char* normalExt, const char* inverseExt)
    : type(type), isEncrypted(isEncrypted), normalExt(normalExt), inverseExt(inverseExt), extValue(getExtValue(normalExt)) {}

const ShadyCore::FileType ShadyCore::FileType::typeUnknown(TYPE_UNKNOWN, false);
const ShadyCore::FileType ShadyCore::FileType::typePackage(TYPE_PACKAGE, false);
const ShadyCore::FileType ShadyCore::FileType::typeDText(TYPE_TEXT, false, ".txt", ".cv0");
const ShadyCore::FileType ShadyCore::FileType::typeDTable(TYPE_TEXT, false, ".csv", ".cv1");
const ShadyCore::FileType ShadyCore::FileType::typeDLabel(TYPE_LABEL, false, ".lbl", ".sfl");
const ShadyCore::FileType ShadyCore::FileType::typeDImage(TYPE_IMAGE, false, ".png", ".cv2");
const ShadyCore::FileType ShadyCore::FileType::typeDPalette(TYPE_PALETTE, false, ".act", ".pal");
const ShadyCore::FileType ShadyCore::FileType::typeDSfx(TYPE_SFX, false, ".wav", ".cv3");
const ShadyCore::FileType ShadyCore::FileType::typeDGui(TYPE_GUI, false, ".xml", ".dat");
const ShadyCore::FileType ShadyCore::FileType::typeDAnimation(TYPE_ANIMATION, false, ".xml", ".pat");
const ShadyCore::FileType ShadyCore::FileType::typeDPattern(TYPE_PATTERN, false, ".xml", ".pat");
const ShadyCore::FileType ShadyCore::FileType::typeEText(TYPE_TEXT, true, ".cv0", ".txt");
const ShadyCore::FileType ShadyCore::FileType::typeETable(TYPE_TEXT, true, ".cv1", ".csv");
const ShadyCore::FileType ShadyCore::FileType::typeELabel(TYPE_LABEL, true, ".sfl", ".lbl");
const ShadyCore::FileType ShadyCore::FileType::typeEImage(TYPE_IMAGE, true, ".cv2", ".png");
const ShadyCore::FileType ShadyCore::FileType::typeEPalette(TYPE_PALETTE, true, ".pal", ".act");
const ShadyCore::FileType ShadyCore::FileType::typeESfx(TYPE_SFX, true, ".cv3", ".wav");
const ShadyCore::FileType ShadyCore::FileType::typeEGui(TYPE_GUI, true, ".dat", ".xml");
const ShadyCore::FileType ShadyCore::FileType::typeEAnimation(TYPE_ANIMATION, true, ".pat", ".xml");
const ShadyCore::FileType ShadyCore::FileType::typeEPattern(TYPE_PATTERN, true, ".pat", ".xml");

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
    // TODO remaining
    if (strcmp(name, "lbl") == 0
        || strcmp(name, "label") == 0) return typeDLabel;
    if (strcmp(name, "png") == 0) return typeDImage;
    if (strcmp(name, "act") == 0) return typeDPalette;
    if (strcmp(name, "wav") == 0) return typeDSfx;
    return typeUnknown;
}

ShadyCore::Resource* ShadyCore::createResource(const FileType& type) {
    switch(type.type) {
	case FileType::TYPE_TEXT: return new TextResource();
	case FileType::TYPE_LABEL: return new LabelResource();
	case FileType::TYPE_PALETTE: return new Palette();
    case FileType::TYPE_IMAGE: return new Image();
	case FileType::TYPE_SFX: return new Sfx();
    case FileType::TYPE_GUI: return new GuiRoot();
    case FileType::TYPE_PATTERN: return new Pattern(false);
    case FileType::TYPE_ANIMATION: return new Pattern(true);
	default: return 0;
    }
}

ShadyCore::Resource* ShadyCore::readResource(const FileType& type, std::istream& input) {
	Resource* resource = createResource(type);
	if (resource) {
		if (type.isEncrypted) ResourceEReader(resource, input);
		else ResourceDReader(resource, input);
	} return resource;
}

//ShadyCore::Resource* ShadyCore::readResource(BasePackageEntry& entry) {
//	std::istream& input = entry.open();
//	ShadyCore::Resource* resource = readResource(FileType::get(entry.getName(), input), input);
//	entry.close();
//	return resource;
//}

void ShadyCore::readResource(Resource* resource, std::istream& input, bool isEncrypted) {
	if (isEncrypted) ResourceEReader(resource, input);
    else ResourceDReader(resource, input);
}

void ShadyCore::writeResource(Resource* resource, std::ostream& output, bool isEncrypted) {
	if (isEncrypted) ResourceEWriter(resource, output);
	else ResourceDWriter(resource, output);
}

void ShadyCore::convertResource(const FileType& type, std::istream& input, std::ostream& output) {
	Resource* resource = createResource(type);
	if (!resource) throw; // TODO Error Handling
	if (type.isEncrypted) {
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
