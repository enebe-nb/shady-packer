#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include "../util/xmlprinter.hpp"
#include <rapidxml.hpp>
#include <cstring>

ShadyCore::Frame::~Frame() {
    if (impl) delete impl;
    if (imageName) delete[] imageName;
}

void ShadyCore::Frame::initialize(const char* n, uint32_t i) {
    index = i;
    size_t length = strlen(n);

    if (impl) delete impl;
    if (root->isAnimationOnly()) {
        type = FRAME_ANIMATION;
        impl = new Animation(*this);
    } else {
        type = FRAME_MOVE;
        impl = new Move(*this);
    }

    if (imageName) delete[] imageName;
	imageName = new char[length + 1];
	strcpy((char*)imageName, n);
}

void ShadyCore::Frame::setImageName(const char* n) {
    size_t length = strlen(n);

    if (imageName) delete[] imageName;
	imageName = new char[length + 1];
	strcpy((char*)imageName, n);
}

ShadyCore::Sequence::~Sequence() {
    if (impl) delete impl;
}

void ShadyCore::Sequence::initialize(uint32_t i, int32_t index) {
	id = i;
	if (impl) delete impl;
    if (index == -1) {
        type = SEQUENCE_CLONE;
        impl = new Clone(*this);
    } else if (root->isAnimationOnly()) {
        type = SEQUENCE_ANIMATION;
        impl = new Animation(*this, index);
    } else {
        type = SEQUENCE_MOVE;
        impl = new Move(*this, index);
    }
}

ShadyCore::Sequence::Impl::~Impl() {}
ShadyCore::Sequence::Animation::~Animation() {
    for (auto frame : frames) delete frame;
}

ShadyCore::Pattern::Pattern(bool a) {
    initialize(a);
}

ShadyCore::Pattern::~Pattern() {
    for (auto sequence : sequences) delete sequence;
}

void ShadyCore::Pattern::initialize(bool a) {
	animationOnly = a;

	for (auto sequence : sequences) delete sequence;
	sequences.clear();
}

namespace {
	class StreamData : public ShadyCore::ResourceVisitor::TempData {
	public:
		std::vector<const char*> imageNames;
		uint32_t lastId = -1;
		uint32_t lastIndex = -1;
		uint32_t curFrameIndex = -1;
		inline StreamData(uint32_t count) { imageNames.reserve(count); }
		inline ~StreamData() { for (auto name : imageNames) delete[] name; }
	};

	class XmlReaderData : public ShadyCore::ResourceVisitor::TempData {
	private:
		rapidxml::xml_document<> document;
		char* data;
	public:
		rapidxml::xml_node<>* current;
		inline XmlReaderData(std::istream& input) {
			input.seekg(0, std::ios::end);
			size_t len = input.tellg();
			input.seekg(0);

			data = new char[len + 1];
			data[len] = '\0';
			input.read(data, len);

			document.parse<0>(data);
			current = document.first_node();
		}
		inline ~XmlReaderData() { delete[] data; }
	};

	class XmlWriterData : public ShadyCore::ResourceVisitor::TempData {
	public:
		ShadyUtil::XmlPrinter printer;
		inline XmlWriterData(std::ostream& output) : printer(output) {}
	};
}

const char* const ShadyCore::MoveTraits::comboModifierNames[] = {
    "liftmodifier",  "smashmodifier",   "bordermodifier", "chainmodifier",
    "spellmodifier", "countermodifier", "modifier07",     "modifier08",
};

const char* const ShadyCore::MoveTraits::frameFlagNames[] = {
    "standing",             "crouching",          "airborne",           "down",
    "guardcancel",          "cancellable",        "takecounterhit",     "superarmor",
    "normalarmor",          "fflag10GUARD_POINT", "grazing",            "fflag12GUARDING",
    "grabinvul",            "melleinvul",         "bulletinvul",        "airattackinvul",
    "highattackinvul",      "lowattackinvul",     "objectattackinvul",  "fflag20REFLECT_BULLET",
    "fflag21FLIP_VELOCITY", "movimentcancel",     "fflag23AFTER_IMAGE", "fflag24LOOP_ANIMATION",
    "fflag25ATTACK_OBJECT", "fflag26",            "fflag27",            "fflag28",
    "fflag29",              "fflag30",            "fflag31",            "fflag32"
};

const char* const ShadyCore::MoveTraits::attackFlagNames[] = {
    "aflag01STAGGER",         "highrightblock",       "lowrightblock",      "airblockable",
    "unblockable",            "ignorearmor",          "hitonwrongblock",    "aflag08GRAB",
    "aflag09",                "aflag10",              "inducecounterhit",   "skillattacktype",
    "spellattacktype",        "airattacktype",        "highattacktype",     "lowattacktype",
    "objectattacktype",       "aflag18UNREFLECTABLE", "aflag19",            "guardcrush",
    "friendlyfire",           "aflag22stagger",       "isbullet",           "ungrazeble",
    "aflag25DRAINS_ON_GRAZE", "aflag26",              "aflag27",            "aflag28",
    "aflag29",                "aflag30",              "aflag31",            "aflag32"
};

static inline void eraseExcept(ShadyCore::Pattern* root, uint32_t id, const ShadyCore::Sequence* except) {
	uint32_t count, beg, end, i = 0;
    while(i < (count = root->getSequenceCount())) {
        ShadyCore::Sequence* seq = &root->getSequence(beg = end = i);
        while(seq->getId() == id && seq != except && ++end < count) {
            seq = &root->getSequence(end);
        }
        if (beg == end) ++i;
        else root->eraseSequence(beg, end);
    }
}

void ShadyCore::ResourceDReader::accept(BlendOptions& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	ShadyUtil::readX(readerData->current, "mode", resource, &BlendOptions::setMode);
	ShadyUtil::readX(readerData->current, "color", resource, &BlendOptions::setColor, true);
	ShadyUtil::readX(readerData->current, "xscale", resource, &BlendOptions::setScaleX);
	ShadyUtil::readX(readerData->current, "yscale", resource, &BlendOptions::setScaleY);
	ShadyUtil::readX(readerData->current, "vertflip", resource, &BlendOptions::setFlipVert);
	ShadyUtil::readX(readerData->current, "horzflip", resource, &BlendOptions::setFlipHorz);
	ShadyUtil::readX(readerData->current, "angle", resource, &BlendOptions::setAngle);
}

void ShadyCore::ResourceEReader::accept(BlendOptions& resource) {
	ShadyUtil::readS(input, resource, &BlendOptions::setMode);
	ShadyUtil::readS(input, resource, &BlendOptions::setColor);
	ShadyUtil::readS(input, resource, &BlendOptions::setScaleX);
	ShadyUtil::readS(input, resource, &BlendOptions::setScaleY);
	ShadyUtil::readS(input, resource, &BlendOptions::setFlipVert);
	ShadyUtil::readS(input, resource, &BlendOptions::setFlipHorz);
	ShadyUtil::readS(input, resource, &BlendOptions::setAngle);
}

void ShadyCore::ResourceDReader::accept(MoveTraits& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	ShadyUtil::readX(readerData->current, "damage", resource, &MoveTraits::setDamage);
	ShadyUtil::readX(readerData->current, "proration", resource, &MoveTraits::setProration);
	ShadyUtil::readX(readerData->current, "chipdamage", resource, &MoveTraits::setChipDamage);
	ShadyUtil::readX(readerData->current, "spiritdamage", resource, &MoveTraits::setSpiritDamage);
	ShadyUtil::readX(readerData->current, "untech", resource, &MoveTraits::setUntech);
	ShadyUtil::readX(readerData->current, "power", resource, &MoveTraits::setPower);
	ShadyUtil::readX(readerData->current, "limit", resource, &MoveTraits::setLimit);
	ShadyUtil::readX(readerData->current, "onhitplayerstun", resource, &MoveTraits::setOnHitPlayerStun);
	ShadyUtil::readX(readerData->current, "onhitenemystun", resource, &MoveTraits::setOnHitEnemyStun);
	ShadyUtil::readX(readerData->current, "onblockplayerstun", resource, &MoveTraits::setOnBlockPlayerStun);
	ShadyUtil::readX(readerData->current, "onblockenemystun", resource, &MoveTraits::setOnBlockEnemyStun);
	ShadyUtil::readX(readerData->current, "onhitcardgain", resource, &MoveTraits::setOnHitCardGain);
	ShadyUtil::readX(readerData->current, "onblockcardgain", resource, &MoveTraits::setOnBlockCardGain);
	ShadyUtil::readX(readerData->current, "onairhitsetsequence", resource, &MoveTraits::setOnAirHitSetSequence);
	ShadyUtil::readX(readerData->current, "ongroundhitsetsequence", resource, &MoveTraits::setOnGroundHitSetSequence);
	ShadyUtil::readX(readerData->current, "xspeed", resource, &MoveTraits::setSpeedX);
	ShadyUtil::readX(readerData->current, "yspeed", resource, &MoveTraits::setSpeedY);
	ShadyUtil::readX(readerData->current, "onhitsfx", resource, &MoveTraits::setOnHitSfx);
	ShadyUtil::readX(readerData->current, "onhiteffect", resource, &MoveTraits::setOnHitEffect);
	ShadyUtil::readX(readerData->current, "attacklevel", resource, &MoveTraits::setAttackLevel);

	resource.setComboModifier(0);
	resource.setFrameFlags(0);
	resource.setAttackFlags(0);
	for (rapidxml::xml_node<>* iter = readerData->current->first_node(); iter; iter = iter->next_sibling()) {
		for (int i = 0; i < 32; ++i) {
			if (i < 8 && strcmp(iter->name(), MoveTraits::comboModifierNames[i]) == 0) {
				resource.setComboModifier(resource.getComboModifier() | 1 << i);
				break;
			}
			if (strcmp(iter->name(), MoveTraits::frameFlagNames[i]) == 0) {
				resource.setFrameFlags(resource.getFrameFlags() | 1 << i);
				break;
			}
			if (strcmp(iter->name(), MoveTraits::attackFlagNames[i]) == 0) {
				resource.setAttackFlags(resource.getAttackFlags() | 1 << i);
				break;
			}
		}
	}
}

void ShadyCore::ResourceEReader::accept(MoveTraits& resource) {
    ShadyUtil::readS(input, resource, &MoveTraits::setDamage);
    ShadyUtil::readS(input, resource, &MoveTraits::setProration);
    ShadyUtil::readS(input, resource, &MoveTraits::setChipDamage);
    ShadyUtil::readS(input, resource, &MoveTraits::setSpiritDamage);
    ShadyUtil::readS(input, resource, &MoveTraits::setUntech);
    ShadyUtil::readS(input, resource, &MoveTraits::setPower);
    ShadyUtil::readS(input, resource, &MoveTraits::setLimit);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnHitPlayerStun);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnHitEnemyStun);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnBlockPlayerStun);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnBlockEnemyStun);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnHitCardGain);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnBlockCardGain);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnAirHitSetSequence);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnGroundHitSetSequence);
    ShadyUtil::readS(input, resource, &MoveTraits::setSpeedX);
    ShadyUtil::readS(input, resource, &MoveTraits::setSpeedY);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnHitSfx);
    ShadyUtil::readS(input, resource, &MoveTraits::setOnHitEffect);
    ShadyUtil::readS(input, resource, &MoveTraits::setAttackLevel);
    ShadyUtil::readS(input, resource, &MoveTraits::setComboModifier);
    ShadyUtil::readS(input, resource, &MoveTraits::setFrameFlags);
    ShadyUtil::readS(input, resource, &MoveTraits::setAttackFlags);
}

void ShadyCore::ResourceDReader::accept(BBoxList& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	resource.initialize();
	if (readerData->current) for (rapidxml::xml_node<>* iter = readerData->current->first_node(); iter; iter = iter->next_sibling()) {
		BBoxList::BBox& box = resource.createBox();
		ShadyUtil::readX(iter, "left", box, &BBoxList::BBox::setLeft);
		ShadyUtil::readX(iter, "up", box, &BBoxList::BBox::setUp);
		ShadyUtil::readX(iter, "right", box, &BBoxList::BBox::setRight);
		ShadyUtil::readX(iter, "down", box, &BBoxList::BBox::setDown);
		if (resource.isAttack())
			ShadyUtil::readX(iter, "unknown", box, &BBoxList::BBox::setUnknown, true);
	}
}

void ShadyCore::ResourceEReader::accept(BBoxList& resource) {
	resource.initialize();
	uint8_t count; input.read((char*)&count, 1);
	for (int i = 0; i < count; ++i) {
		BBoxList::BBox& box = resource.createBox();
		ShadyUtil::readS(input, box, &BBoxList::BBox::setLeft);
		ShadyUtil::readS(input, box, &BBoxList::BBox::setUp);
		ShadyUtil::readS(input, box, &BBoxList::BBox::setRight);
		ShadyUtil::readS(input, box, &BBoxList::BBox::setDown);
		if (resource.isAttack())
			ShadyUtil::readS(input, box, &BBoxList::BBox::setUnknown);
	}
}

void ShadyCore::ResourceDReader::accept(MoveEffect& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	ShadyUtil::readX(readerData->current, "xpivot", resource, &MoveEffect::setPivotX);
	ShadyUtil::readX(readerData->current, "ypivot", resource, &MoveEffect::setPivotY);
	ShadyUtil::readX(readerData->current, "xpositionextra", resource, &MoveEffect::setPositionXExtra);
	ShadyUtil::readX(readerData->current, "ypositionextra", resource, &MoveEffect::setPositionYExtra);
	ShadyUtil::readX(readerData->current, "xposition", resource, &MoveEffect::setPositionX);
	ShadyUtil::readX(readerData->current, "yposition", resource, &MoveEffect::setPositionY);
	ShadyUtil::readX(readerData->current, "unknown02", resource, &MoveEffect::setUnknown02, true);
	ShadyUtil::readX(readerData->current, "xspeed", resource, &MoveEffect::setSpeedX);
	ShadyUtil::readX(readerData->current, "yspeed", resource, &MoveEffect::setSpeedY);
}

void ShadyCore::ResourceEReader::accept(MoveEffect& resource) {
	ShadyUtil::readS(input, resource, &MoveEffect::setPivotX);
	ShadyUtil::readS(input, resource, &MoveEffect::setPivotY);
	ShadyUtil::readS(input, resource, &MoveEffect::setPositionXExtra);
	ShadyUtil::readS(input, resource, &MoveEffect::setPositionYExtra);
	ShadyUtil::readS(input, resource, &MoveEffect::setPositionX);
	ShadyUtil::readS(input, resource, &MoveEffect::setPositionY);
	ShadyUtil::readS(input, resource, &MoveEffect::setUnknown02);
	ShadyUtil::readS(input, resource, &MoveEffect::setSpeedX);
	ShadyUtil::readS(input, resource, &MoveEffect::setSpeedY);
}

void ShadyCore::ResourceDReader::accept(Frame& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	resource.initialize(readerData->current->first_attribute("image")->value(), atoi(readerData->current->first_attribute("index")->value()));
}

void ShadyCore::ResourceEReader::accept(Frame& resource) {
	if (!tempData) throw; // TODO better way
	StreamData* streamData = (StreamData*)tempData;

	uint16_t index; input.read((char*)&index, 2);
	resource.initialize(streamData->imageNames[index], streamData->curFrameIndex++);
}

void ShadyCore::ResourceDReader::accept(Frame::Animation& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	ShadyUtil::readX(readerData->current, "unknown", resource, &Frame::Animation::setUnknown, true);
	ShadyUtil::readX(readerData->current, "xtexoffset", resource, &Frame::Animation::setTexOffsetX);
	ShadyUtil::readX(readerData->current, "ytexoffset", resource, &Frame::Animation::setTexOffsetY);
	ShadyUtil::readX(readerData->current, "texwidth", resource, &Frame::Animation::setTexWidth);
	ShadyUtil::readX(readerData->current, "texheight", resource, &Frame::Animation::setTexHeight);
	ShadyUtil::readX(readerData->current, "xoffset", resource, &Frame::Animation::setOffsetX);
	ShadyUtil::readX(readerData->current, "yoffset", resource, &Frame::Animation::setOffsetY);
	ShadyUtil::readX(readerData->current, "duration", resource, &Frame::Animation::setDuration);
    ShadyUtil::readX(readerData->current, "rendergroup", resource, &Frame::Animation::setRenderGroup);

	if (resource.hasBlendOptions()) {
		readerData->current = readerData->current->first_node("blend");
		resource.getBlendOptions().visit(this);
		readerData->current = readerData->current->parent();
	}
}

void ShadyCore::ResourceEReader::accept(Frame::Animation& resource) {
	ShadyUtil::readS(input, resource, &Frame::Animation::setUnknown);
	ShadyUtil::readS(input, resource, &Frame::Animation::setTexOffsetX);
	ShadyUtil::readS(input, resource, &Frame::Animation::setTexOffsetY);
	ShadyUtil::readS(input, resource, &Frame::Animation::setTexWidth);
	ShadyUtil::readS(input, resource, &Frame::Animation::setTexHeight);
	ShadyUtil::readS(input, resource, &Frame::Animation::setOffsetX);
	ShadyUtil::readS(input, resource, &Frame::Animation::setOffsetY);
	ShadyUtil::readS(input, resource, &Frame::Animation::setDuration);
	ShadyUtil::readS(input, resource, &Frame::Animation::setRenderGroup);

	if (resource.hasBlendOptions())
		resource.getBlendOptions().visit(this);
}

void ShadyCore::ResourceDReader::accept(Frame::Move& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;
	rapidxml::xml_node<>* current = readerData->current;

	this->accept((Frame::Animation&)resource);
	readerData->current = current->first_node("traits");	resource.getTraits().visit(this);
	readerData->current = current->first_node("collision"); resource.getCollisionBoxes().visit(this);
	readerData->current = current->first_node("hit");		resource.getHitBoxes().visit(this);
	readerData->current = current->first_node("attack");	resource.getAttackBoxes().visit(this);
	readerData->current = current->first_node("effect");	resource.getEffect().visit(this);
	readerData->current = current;
}

void ShadyCore::ResourceEReader::accept(Frame::Move& resource) {
	this->accept((Frame::Animation&)resource);
	resource.getTraits().visit(this);
	resource.getCollisionBoxes().visit(this);
	resource.getHitBoxes().visit(this);
	resource.getAttackBoxes().visit(this);
	resource.getEffect().visit(this);
}

void ShadyCore::ResourceDReader::accept(Sequence& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	resource.initialize(atoi(readerData->current->first_attribute("id")->value()),
        strcmp(readerData->current->name(), "clone") == 0
        ? -1 : atoi(readerData->current->first_attribute("index")->value()));
}

void ShadyCore::ResourceEReader::accept(Sequence& resource) {
	if (!tempData) throw; // TODO better way
	StreamData* streamData = (StreamData*)tempData;

	int32_t id; input.read((char*)&id, 4);
	if (id == -1) {
		input.read((char*)&id, 4);
		resource.initialize(id, -1);
	}
	else if (id == -2) {
		resource.initialize(streamData->lastId, ++streamData->lastIndex);
	}
	else {
		eraseExcept(resource.getRoot(), id, &resource);
		resource.initialize(streamData->lastId = id, streamData->lastIndex = 0);
	} streamData->curFrameIndex = 0;
}

void ShadyCore::ResourceDReader::accept(Sequence::Clone& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;

	ShadyUtil::readX(readerData->current, "target", resource, &Sequence::Clone::setTargetId);
}

void ShadyCore::ResourceEReader::accept(Sequence::Clone& resource) {
	uint32_t target;
	input.read((char*)&target, 4);
	resource.setTargetId(target);
}

void ShadyCore::ResourceDReader::accept(Sequence::Animation& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;
	rapidxml::xml_node<>* current = readerData->current;

	ShadyUtil::readX(readerData->current, "loop", resource, &Sequence::Animation::setLoop);
	for (rapidxml::xml_node<>* iter = readerData->current = readerData->current->first_node(); iter; iter = readerData->current = iter->next_sibling()) {
		resource.createFrame().visit(this);
	} readerData->current = current;
}

void ShadyCore::ResourceEReader::accept(Sequence::Animation& resource) {
	ShadyUtil::readS(input, resource, &Sequence::Animation::setLoop);

	uint32_t count; input.read((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.createFrame().visit(this);
	}
}

void ShadyCore::ResourceDReader::accept(Sequence::Move& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;
	rapidxml::xml_node<>* current = readerData->current;

	ShadyUtil::readX(readerData->current, "loop", (Sequence::Animation&)resource, &Sequence::Animation::setLoop);
	ShadyUtil::readX(readerData->current, "movelock", resource, &Sequence::Move::setMoveLock);
	ShadyUtil::readX(readerData->current, "actionlock", resource, &Sequence::Move::setActionLock);
	for (rapidxml::xml_node<>* iter = readerData->current = readerData->current->first_node(); iter; iter = readerData->current = iter->next_sibling()) {
		resource.createFrame().visit(this);
	} readerData->current = current;
}

void ShadyCore::ResourceEReader::accept(Sequence::Move& resource) {
	ShadyUtil::readS(input, resource, &Sequence::Move::setMoveLock);
	ShadyUtil::readS(input, resource, &Sequence::Move::setActionLock);
	ShadyUtil::readS(input, (Sequence::Animation&)resource, &Sequence::Animation::setLoop);

	uint32_t count; input.read((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.createFrame().visit(this);
	}
}

void ShadyCore::ResourceDReader::accept(Pattern& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*)tempData;
	char* c = readerData->current->name(); while(*c) *c++ = tolower(*c);
	resource.initialize(strcmp(readerData->current->name(), "animpattern") == 0);

	for (rapidxml::xml_node<>* iter = readerData->current = readerData->current->first_node(); iter; iter = readerData->current = iter->next_sibling()) {
		resource.createSequence().visit(this);
	}
}

void ShadyCore::ResourceEReader::accept(Pattern& resource) {
	uint32_t count = 0;
	input.ignore(1);
	input.read((char*)&count, 2);

	if (!tempData) tempData = new StreamData(count); // TODO better way
	StreamData* streamData = (StreamData*)tempData;

	for (int i = 0; i < count; ++i) {
		char* buffer = new char[128];
		input.read(buffer, 128);
		streamData->imageNames.push_back(buffer);
	}


	input.read((char*)&count, 4);
	input.peek(); while (!input.eof()) {
		resource.createSequence().visit(this);
		input.peek();
	}
}

void ShadyCore::ResourceDWriter::accept(BlendOptions& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode("blend");
	writerData->printer.appendAttribute("mode", resource.getMode());
	writerData->printer.appendAttribute("color", resource.getColor(), true);
	writerData->printer.appendAttribute("xscale", resource.getScaleX());
	writerData->printer.appendAttribute("yscale", resource.getScaleY());
	writerData->printer.appendAttribute("vertflip", resource.getFlipVert());
	writerData->printer.appendAttribute("horzflip", resource.getFlipHorz());
	writerData->printer.appendAttribute("angle", resource.getAngle());
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(BlendOptions& resource) {
	ShadyUtil::writeS(output, resource.getMode());
	ShadyUtil::writeS(output, resource.getColor());
	ShadyUtil::writeS(output, resource.getScaleX());
	ShadyUtil::writeS(output, resource.getScaleY());
	ShadyUtil::writeS(output, resource.getFlipVert());
	ShadyUtil::writeS(output, resource.getFlipHorz());
	ShadyUtil::writeS(output, resource.getAngle());
}

void ShadyCore::ResourceDWriter::accept(MoveTraits& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode("traits");
	writerData->printer.appendAttribute("damage", resource.getDamage());
	writerData->printer.appendAttribute("proration", resource.getProration());
	writerData->printer.appendAttribute("chipdamage", resource.getChipDamage());
	writerData->printer.appendAttribute("spiritdamage", resource.getSpiritDamage());
	writerData->printer.appendAttribute("untech", resource.getUntech());
	writerData->printer.appendAttribute("power", resource.getPower());
	writerData->printer.appendAttribute("limit", resource.getLimit());
	writerData->printer.appendAttribute("onhitplayerstun", resource.getOnHitPlayerStun());
	writerData->printer.appendAttribute("onhitenemystun", resource.getOnHitEnemyStun());
	writerData->printer.appendAttribute("onblockplayerstun", resource.getOnBlockPlayerStun());
	writerData->printer.appendAttribute("onblockenemystun", resource.getOnBlockEnemyStun());
	writerData->printer.appendAttribute("onhitcardgain", resource.getOnHitCardGain());
	writerData->printer.appendAttribute("onblockcardgain", resource.getOnBlockCardGain());
	writerData->printer.appendAttribute("onairhitsetsequence", resource.getOnAirHitSetSequence());
	writerData->printer.appendAttribute("ongroundhitsetsequence", resource.getOnGroundHitSetSequence());
	writerData->printer.appendAttribute("xspeed", resource.getSpeedX());
	writerData->printer.appendAttribute("yspeed", resource.getSpeedY());
	writerData->printer.appendAttribute("onhitsfx", resource.getOnHitSfx());
	writerData->printer.appendAttribute("onhiteffect", resource.getOnHitEffect());
	writerData->printer.appendAttribute("attacklevel", resource.getAttackLevel());

	for (int i = 0; i < 32; ++i) {
		if (i < 8 && (resource.getComboModifier() >> i & 1)) {
			writerData->printer.openNode(MoveTraits::comboModifierNames[i]);
			writerData->printer.closeNode();
		} if (resource.getFrameFlags() >> i & 1) {
			writerData->printer.openNode(MoveTraits::frameFlagNames[i]);
			writerData->printer.closeNode();
		} if (resource.getAttackFlags() >> i & 1) {
			writerData->printer.openNode(MoveTraits::attackFlagNames[i]);
			writerData->printer.closeNode();
		}
	}

	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(MoveTraits& resource) {
	ShadyUtil::writeS(output, resource.getDamage());
	ShadyUtil::writeS(output, resource.getProration());
	ShadyUtil::writeS(output, resource.getChipDamage());
	ShadyUtil::writeS(output, resource.getSpiritDamage());
	ShadyUtil::writeS(output, resource.getUntech());
	ShadyUtil::writeS(output, resource.getPower());
	ShadyUtil::writeS(output, resource.getLimit());
	ShadyUtil::writeS(output, resource.getOnHitPlayerStun());
	ShadyUtil::writeS(output, resource.getOnHitEnemyStun());
	ShadyUtil::writeS(output, resource.getOnBlockPlayerStun());
	ShadyUtil::writeS(output, resource.getOnBlockEnemyStun());
	ShadyUtil::writeS(output, resource.getOnHitCardGain());
	ShadyUtil::writeS(output, resource.getOnBlockCardGain());
	ShadyUtil::writeS(output, resource.getOnAirHitSetSequence());
	ShadyUtil::writeS(output, resource.getOnGroundHitSetSequence());
	ShadyUtil::writeS(output, resource.getSpeedX());
	ShadyUtil::writeS(output, resource.getSpeedY());
	ShadyUtil::writeS(output, resource.getOnHitSfx());
	ShadyUtil::writeS(output, resource.getOnHitEffect());
	ShadyUtil::writeS(output, resource.getAttackLevel());
	ShadyUtil::writeS(output, resource.getComboModifier());
	ShadyUtil::writeS(output, resource.getFrameFlags());
	ShadyUtil::writeS(output, resource.getAttackFlags());
}

void ShadyCore::ResourceDWriter::accept(BBoxList& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	uint8_t count = resource.getBoxCount();
	for (int i = 0; i < count; ++i) {
		BBoxList::BBox& box = resource.getBox(i);
		writerData->printer.openNode("box");
		writerData->printer.appendAttribute("left", box.getLeft());
		writerData->printer.appendAttribute("up", box.getUp());
		writerData->printer.appendAttribute("right", box.getRight());
		writerData->printer.appendAttribute("down", box.getDown());
		if (resource.isAttack())
			writerData->printer.appendAttribute("unknown", box.getUnknown());
		writerData->printer.closeNode();
	}
}

void ShadyCore::ResourceEWriter::accept(BBoxList& resource) {
	uint8_t count = resource.getBoxCount(); output.write((char*)&count, 1);
	for (int i = 0; i < count; ++i) {
		BBoxList::BBox& box = resource.getBox(i);
		ShadyUtil::writeS(output, box.getLeft());
		ShadyUtil::writeS(output, box.getUp());
		ShadyUtil::writeS(output, box.getRight());
		ShadyUtil::writeS(output, box.getDown());
		if (resource.isAttack())
			ShadyUtil::writeS(output, box.getUnknown());
	}
}

void ShadyCore::ResourceDWriter::accept(MoveEffect& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode("effect");
	writerData->printer.appendAttribute("xpivot", resource.getPivotX());
	writerData->printer.appendAttribute("ypivot", resource.getPivotY());
	writerData->printer.appendAttribute("xpositionextra", resource.getPositionXExtra());
	writerData->printer.appendAttribute("ypositionextra", resource.getPositionYExtra());
	writerData->printer.appendAttribute("xposition", resource.getPositionX());
	writerData->printer.appendAttribute("yposition", resource.getPositionY());
	writerData->printer.appendAttribute("unknown02", resource.getUnknown02(), true);
	writerData->printer.appendAttribute("xspeed", resource.getSpeedX());
	writerData->printer.appendAttribute("yspeed", resource.getSpeedY());
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(MoveEffect& resource) {
	ShadyUtil::writeS(output, resource.getPivotX());
	ShadyUtil::writeS(output, resource.getPivotY());
	ShadyUtil::writeS(output, resource.getPositionXExtra());
	ShadyUtil::writeS(output, resource.getPositionYExtra());
	ShadyUtil::writeS(output, resource.getPositionX());
	ShadyUtil::writeS(output, resource.getPositionY());
	ShadyUtil::writeS(output, resource.getUnknown02());
	ShadyUtil::writeS(output, resource.getSpeedX());
	ShadyUtil::writeS(output, resource.getSpeedY());
}

void ShadyCore::ResourceDWriter::accept(Frame& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode("frame");
	writerData->printer.appendAttribute("image", resource.getImageName());
	writerData->printer.appendAttribute("index", resource.getIndex());
}

void ShadyCore::ResourceEWriter::accept(Frame& resource) {
	if (!tempData) throw; // TODO better way
	StreamData* streamData = (StreamData*)tempData;

	uint16_t index = 0; while (strcmp(resource.getImageName(), streamData->imageNames[index]) != 0) ++index;
	output.write((char*)&index, 2);
}

void ShadyCore::ResourceDWriter::accept(Frame::Animation& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("unknown", resource.getUnknown(), true);
	writerData->printer.appendAttribute("xtexoffset", resource.getTexOffsetX());
	writerData->printer.appendAttribute("ytexoffset", resource.getTexOffsetY());
	writerData->printer.appendAttribute("texwidth", resource.getTexWidth());
	writerData->printer.appendAttribute("texheight", resource.getTexHeight());
	writerData->printer.appendAttribute("xoffset", resource.getOffsetX());
	writerData->printer.appendAttribute("yoffset", resource.getOffsetY());
	writerData->printer.appendAttribute("duration", resource.getDuration());
    writerData->printer.appendAttribute("rendergroup", resource.getRenderGroup());

    if (resource.hasBlendOptions())
        resource.getBlendOptions().visit(this);

	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(Frame::Animation& resource) {
	ShadyUtil::writeS(output, resource.getUnknown());
	ShadyUtil::writeS(output, resource.getTexOffsetX());
	ShadyUtil::writeS(output, resource.getTexOffsetY());
	ShadyUtil::writeS(output, resource.getTexWidth());
	ShadyUtil::writeS(output, resource.getTexHeight());
	ShadyUtil::writeS(output, resource.getOffsetX());
	ShadyUtil::writeS(output, resource.getOffsetY());
	ShadyUtil::writeS(output, resource.getDuration());
	ShadyUtil::writeS(output, resource.getRenderGroup());

	if (resource.hasBlendOptions())
		resource.getBlendOptions().visit(this);
}

void ShadyCore::ResourceDWriter::accept(Frame::Move& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("unknown", resource.getUnknown(), true);
	writerData->printer.appendAttribute("xtexoffset", resource.getTexOffsetX());
	writerData->printer.appendAttribute("ytexoffset", resource.getTexOffsetY());
	writerData->printer.appendAttribute("texwidth", resource.getTexWidth());
	writerData->printer.appendAttribute("texheight", resource.getTexHeight());
	writerData->printer.appendAttribute("xoffset", resource.getOffsetX());
	writerData->printer.appendAttribute("yoffset", resource.getOffsetY());
	writerData->printer.appendAttribute("duration", resource.getDuration());
    writerData->printer.appendAttribute("rendergroup", resource.getRenderGroup());

    if (resource.hasBlendOptions())
        resource.getBlendOptions().visit(this);

	resource.getTraits().visit(this);
	if (resource.getCollisionBoxes().getBoxCount()) {
		writerData->printer.openNode("collision");
		resource.getCollisionBoxes().visit(this);
		writerData->printer.closeNode();
	} if (resource.getHitBoxes().getBoxCount()) {
		writerData->printer.openNode("hit");
		resource.getHitBoxes().visit(this);
		writerData->printer.closeNode();
	} if (resource.getAttackBoxes().getBoxCount()) {
		writerData->printer.openNode("attack");
		resource.getAttackBoxes().visit(this);
		writerData->printer.closeNode();
	}

	resource.getEffect().visit(this);
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(Frame::Move& resource) {
	this->accept((Frame::Animation&)resource);
	resource.getTraits().visit(this);
	resource.getCollisionBoxes().visit(this);
	resource.getHitBoxes().visit(this);
	resource.getAttackBoxes().visit(this);
	resource.getEffect().visit(this);
}

static const char* typeNames[] = {
	"clone",
	"animation",
	"move",
};

void ShadyCore::ResourceDWriter::accept(Sequence& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode(typeNames[resource.getType()]);
	writerData->printer.appendAttribute("id", resource.getId());
}

void ShadyCore::ResourceEWriter::accept(Sequence& resource) {
	if (!tempData) throw; // TODO better way
	StreamData* streamData = (StreamData*)tempData;

	if (resource.getType() == ShadyCore::Sequence::SEQUENCE_CLONE) {
		ShadyUtil::writeS(output, (int32_t)-1);
		ShadyUtil::writeS(output, resource.getId());
	} else {
		if (resource.getId() == streamData->lastId)
			ShadyUtil::writeS(output, (int32_t)-2);
		else ShadyUtil::writeS(output, resource.getId());
		streamData->lastId = resource.getId();
	}
}

void ShadyCore::ResourceDWriter::accept(Sequence::Clone& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("target", resource.getTargetId());
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(Sequence::Clone& resource) {
	ShadyUtil::writeS(output, resource.getTargetId());
}

void ShadyCore::ResourceDWriter::accept(Sequence::Animation& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("index", resource.getIndex());
	writerData->printer.appendAttribute("loop", resource.getLoop());
	for (uint32_t i = 0; i < resource.getFrameCount(); ++i) {
		resource.getFrame(i).visit(this);
	}
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(Sequence::Animation& resource) {
	ShadyUtil::writeS(output, resource.getLoop());

	uint32_t count = resource.getFrameCount(); output.write((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.getFrame(i).visit(this);
	}
}

void ShadyCore::ResourceDWriter::accept(Sequence::Move& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("index", resource.getIndex());
	writerData->printer.appendAttribute("loop", resource.getLoop());
	writerData->printer.appendAttribute("movelock", resource.getMoveLock());
	writerData->printer.appendAttribute("actionlock", resource.getActionLock());
	for (uint32_t i = 0; i < resource.getFrameCount(); ++i) {
		resource.getFrame(i).visit(this);
	}
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(Sequence::Move& resource) {
	ShadyUtil::writeS(output, resource.getMoveLock());
	ShadyUtil::writeS(output, resource.getActionLock());
	ShadyUtil::writeS(output, resource.getLoop());

	uint32_t count = resource.getFrameCount(); output.write((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.getFrame(i).visit(this);
	}
}

template <class Impl> static inline void collectImageNames(std::vector<const char*>& imageNames, Impl& sequence) {
	for (int i = 0; i < sequence.getFrameCount(); ++i) {
		int j = 0; while (j < imageNames.size() && strcmp(sequence.getFrame(i).getImageName(), imageNames[j]) != 0) ++j;
		if (j >= imageNames.size()) {
            char* buffer = new char[128];
            strcpy(buffer, sequence.getFrame(i).getImageName());
            imageNames.push_back(buffer);
        }
	}
}

void ShadyCore::ResourceDWriter::accept(Pattern& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode(resource.isAnimationOnly() ? "animpattern" : "movepattern");
	writerData->printer.appendAttribute("version", "5");

	for (uint32_t i = 0; i < resource.getSequenceCount(); ++i) {
		resource.getSequence(i).visit(this);
	}

	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(Pattern& resource) {
	output.write("\x05", 1);

	if (!tempData) tempData = new StreamData(100); // TODO better way
	StreamData* streamData = (StreamData*)tempData;

	for (int i = 0; i < resource.getSequenceCount(); ++i) {
		if (resource.getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_ANIMATION)
			collectImageNames(streamData->imageNames, resource.getSequence(i).asAnimation());
		else if (resource.getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_MOVE)
			collectImageNames(streamData->imageNames, resource.getSequence(i).asMove());
	}

	uint16_t count = streamData->imageNames.size();
	ShadyUtil::writeS(output, count);
	for (int i = 0; i < count; ++i) {
		size_t len = strlen(streamData->imageNames[i]);
		output.write(streamData->imageNames[i], len + 1);
		while (++len < 128) output.put(0);
	}

	// TODO Sort sequences and frames

	ShadyUtil::writeS(output, resource.getSequenceCount());
	for (int i = 0; i < resource.getSequenceCount(); ++i) {
		resource.getSequence(i).visit(this);
	}
}
