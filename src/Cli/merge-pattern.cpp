#include "command.hpp"
#include "../Core/util/iohelper.hpp"
#include <fstream>
#include <rapidxml.hpp>
#include <cstring>

namespace {
    class XmlReaderData {
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
}

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

static void processBlend(XmlReaderData* data, ShadyCore::Frame::Animation* frame) {
    rapidxml::xml_node<>* current = data->current;
    if (current->first_attribute("mode"))
        ShadyUtil::readX(current, "mode", frame->getBlendOptions(), &ShadyCore::BlendOptions::setMode);
    if (current->first_attribute("color"))
        ShadyUtil::readX(current, "color", frame->getBlendOptions(), &ShadyCore::BlendOptions::setColor, true);
    if (current->first_attribute("xscale"))
        ShadyUtil::readX(current, "xscale", frame->getBlendOptions(), &ShadyCore::BlendOptions::setScaleX);
    if (current->first_attribute("yscale"))
        ShadyUtil::readX(current, "yscale", frame->getBlendOptions(), &ShadyCore::BlendOptions::setScaleY);
    if (current->first_attribute("vertflip"))
        ShadyUtil::readX(current, "vertflip", frame->getBlendOptions(), &ShadyCore::BlendOptions::setFlipVert);
    if (current->first_attribute("horzflip"))
        ShadyUtil::readX(current, "horzflip", frame->getBlendOptions(), &ShadyCore::BlendOptions::setFlipHorz);
    if (current->first_attribute("angle"))
        ShadyUtil::readX(current, "angle", frame->getBlendOptions(), &ShadyCore::BlendOptions::setAngle);
}

static void processTraits(XmlReaderData* data, ShadyCore::Frame::Move* frame) {
    rapidxml::xml_node<>* current = data->current;
    if (current->first_attribute("damage"))
        ShadyUtil::readX(current, "damage", frame->getTraits(), &ShadyCore::MoveTraits::setDamage);
    if (current->first_attribute("proration"))
        ShadyUtil::readX(current, "proration", frame->getTraits(), &ShadyCore::MoveTraits::setProration);
    if (current->first_attribute("chipdamage"))
        ShadyUtil::readX(current, "chipdamage", frame->getTraits(), &ShadyCore::MoveTraits::setChipDamage);
    if (current->first_attribute("spiritdamage"))
        ShadyUtil::readX(current, "spiritdamage", frame->getTraits(), &ShadyCore::MoveTraits::setSpiritDamage);
    if (current->first_attribute("untech"))
        ShadyUtil::readX(current, "untech", frame->getTraits(), &ShadyCore::MoveTraits::setUntech);
    if (current->first_attribute("power"))
        ShadyUtil::readX(current, "power", frame->getTraits(), &ShadyCore::MoveTraits::setPower);
    if (current->first_attribute("limit"))
        ShadyUtil::readX(current, "limit", frame->getTraits(), &ShadyCore::MoveTraits::setLimit);
    if (current->first_attribute("onhitplayerstun"))
        ShadyUtil::readX(current, "onhitplayerstun", frame->getTraits(), &ShadyCore::MoveTraits::setOnHitPlayerStun);
    if (current->first_attribute("onhitenemystun"))
        ShadyUtil::readX(current, "onhitenemystun", frame->getTraits(), &ShadyCore::MoveTraits::setOnHitEnemyStun);
    if (current->first_attribute("onblockplayerstun"))
        ShadyUtil::readX(current, "onblockplayerstun", frame->getTraits(), &ShadyCore::MoveTraits::setOnBlockPlayerStun);
    if (current->first_attribute("onblockenemystun"))
        ShadyUtil::readX(current, "onblockenemystun", frame->getTraits(), &ShadyCore::MoveTraits::setOnBlockEnemyStun);
    if (current->first_attribute("onhitcardgain"))
        ShadyUtil::readX(current, "onhitcardgain", frame->getTraits(), &ShadyCore::MoveTraits::setOnHitCardGain);
    if (current->first_attribute("onblockcardgain"))
        ShadyUtil::readX(current, "onblockcardgain", frame->getTraits(), &ShadyCore::MoveTraits::setOnBlockCardGain);
    if (current->first_attribute("onairhitsetsequence"))
        ShadyUtil::readX(current, "onairhitsetsequence", frame->getTraits(), &ShadyCore::MoveTraits::setOnAirHitSetSequence);
    if (current->first_attribute("ongroundhitsetsequence"))
        ShadyUtil::readX(current, "ongroundhitsetsequence", frame->getTraits(), &ShadyCore::MoveTraits::setOnGroundHitSetSequence);
    if (current->first_attribute("xspeed"))
        ShadyUtil::readX(current, "xspeed", frame->getTraits(), &ShadyCore::MoveTraits::setSpeedX);
    if (current->first_attribute("yspeed"))
        ShadyUtil::readX(current, "yspeed", frame->getTraits(), &ShadyCore::MoveTraits::setSpeedY);
    if (current->first_attribute("onhitsfx"))
        ShadyUtil::readX(current, "onhitsfx", frame->getTraits(), &ShadyCore::MoveTraits::setOnHitSfx);
    if (current->first_attribute("onhiteffect"))
        ShadyUtil::readX(current, "onhiteffect", frame->getTraits(), &ShadyCore::MoveTraits::setOnHitEffect);
    if (current->first_attribute("attacklevel"))
        ShadyUtil::readX(current, "attacklevel", frame->getTraits(), &ShadyCore::MoveTraits::setAttackLevel);

    if (current->first_attribute("clearall") || current->first_attribute("clearcombomodifier")) frame->getTraits().setComboModifier(0);
    if (current->first_attribute("clearall") || current->first_attribute("clearframeflags")) frame->getTraits().setFrameFlags(0);
    if (current->first_attribute("clearall") || current->first_attribute("clearattackflags")) frame->getTraits().setAttackFlags(0);

	for (rapidxml::xml_node<>* iter = current->first_node(); iter; iter = iter->next_sibling()) {
		for (int i = 0; i < 32; ++i) {
			if (i < 8 && strcmp(iter->name(), ShadyCore::MoveTraits::comboModifierNames[i]) == 0) {
                if (iter->first_attribute("remove"))
                    frame->getTraits().setComboModifier(frame->getTraits().getComboModifier() & ~(1 << i));
                else frame->getTraits().setComboModifier(frame->getTraits().getComboModifier() | 1 << i);
				break;
			}
			if (strcmp(iter->name(), ShadyCore::MoveTraits::frameFlagNames[i]) == 0) {
                if (iter->first_attribute("remove"))
                    frame->getTraits().setFrameFlags(frame->getTraits().getFrameFlags() & ~(1 << i));
                else frame->getTraits().setFrameFlags(frame->getTraits().getFrameFlags() | 1 << i);
				break;
			}
			if (strcmp(iter->name(), ShadyCore::MoveTraits::attackFlagNames[i]) == 0) {
                if (iter->first_attribute("remove"))
                    frame->getTraits().setAttackFlags(frame->getTraits().getAttackFlags() & ~(1 << i));
                else frame->getTraits().setAttackFlags(frame->getTraits().getAttackFlags() | 1 << i);
				break;
			}
		}
	}
}

static void processBoxes(XmlReaderData* data, ShadyCore::BBoxList* boxes) {
    rapidxml::xml_node<>* current = data->current;
    if (current->first_attribute("clear")) boxes->initialize();

    for (current = current->first_node(); current; current = current->next_sibling()){
        uint32_t index = atoi(current->first_attribute("index")->value());

        if (current->first_attribute("remove")) {
            boxes->eraseBox(index);
            continue;
        }

        ShadyCore::BBoxList::BBox& box = index >= boxes->getBoxCount() ? boxes->createBox() : boxes->getBox(index);
        if (current->first_attribute("left"))
            ShadyUtil::readX(current, "left", box, &ShadyCore::BBoxList::BBox::setLeft);
        if (current->first_attribute("up"))
            ShadyUtil::readX(current, "up", box, &ShadyCore::BBoxList::BBox::setUp);
        if (current->first_attribute("right"))
            ShadyUtil::readX(current, "right", box, &ShadyCore::BBoxList::BBox::setRight);
        if (current->first_attribute("down"))
            ShadyUtil::readX(current, "down", box, &ShadyCore::BBoxList::BBox::setDown);
        if (boxes->isAttack() && current->first_attribute("unknown"))
            ShadyUtil::readX(current, "unknown", box, &ShadyCore::BBoxList::BBox::setUnknown, true);
    }
}

static void processEffect(XmlReaderData* data, ShadyCore::Frame::Move* frame) {
    rapidxml::xml_node<>* current = data->current;
    if (current->first_attribute("xpivot"))
        ShadyUtil::readX(current, "xpivot", frame->getEffect(), &ShadyCore::MoveEffect::setPivotX);
    if (current->first_attribute("ypivot"))
        ShadyUtil::readX(current, "ypivot", frame->getEffect(), &ShadyCore::MoveEffect::setPivotY);
    if (current->first_attribute("xpositionextra"))
        ShadyUtil::readX(current, "xpositionextra", frame->getEffect(), &ShadyCore::MoveEffect::setPositionXExtra);
    if (current->first_attribute("ypositionextra"))
        ShadyUtil::readX(current, "ypositionextra", frame->getEffect(), &ShadyCore::MoveEffect::setPositionYExtra);
    if (current->first_attribute("xposition"))
        ShadyUtil::readX(current, "xposition", frame->getEffect(), &ShadyCore::MoveEffect::setPositionX);
    if (current->first_attribute("yposition"))
        ShadyUtil::readX(current, "yposition", frame->getEffect(), &ShadyCore::MoveEffect::setPositionY);
    if (current->first_attribute("unknown02"))
        ShadyUtil::readX(current, "unknown02", frame->getEffect(), &ShadyCore::MoveEffect::setUnknown02, true);
    if (current->first_attribute("xspeed"))
        ShadyUtil::readX(current, "xspeed", frame->getEffect(), &ShadyCore::MoveEffect::setSpeedX);
    if (current->first_attribute("yspeed"))
        ShadyUtil::readX(current, "yspeed", frame->getEffect(), &ShadyCore::MoveEffect::setSpeedY);
}

static void processFrame(XmlReaderData* data, ShadyCore::Sequence::Animation* sequence) {
    rapidxml::xml_node<>* current = data->current;
    uint32_t index = atoi(current->first_attribute("index")->value());

    ShadyCore::Frame* frame = 0;
    uint32_t i; for(i = 0; i < sequence->getFrameCount(); ++i) {
        if (index == sequence->getFrame(i).getIndex()) {
            frame = &sequence->getFrame(i);
            break;
        }
    }

    if (current->first_attribute("remove")) {
        if (frame) sequence->eraseFrame(i);
        return;
    }

    if (!frame) {
        frame = &sequence->createFrame();
        frame->initialize(current->first_attribute("image")->value(), index);
    }

    if (current->first_attribute("replace")) {
        frame->initialize(current->first_attribute("image")->value(), index);
    }

    if (current->first_attribute("image"))
        frame->setImageName(current->first_attribute("image")->value());

    if (frame->getType() == ShadyCore::Frame::FRAME_ANIMATION) {
        if (current->first_attribute("unknown"))
            ShadyUtil::readX(current, "unknown", frame->asAnimation(), &ShadyCore::Frame::Animation::setUnknown, true);
        if (current->first_attribute("xtexoffset"))
            ShadyUtil::readX(current, "xtexoffset", frame->asAnimation(), &ShadyCore::Frame::Animation::setTexOffsetX);
        if (current->first_attribute("ytexoffset"))
            ShadyUtil::readX(current, "ytexoffset", frame->asAnimation(), &ShadyCore::Frame::Animation::setTexOffsetY);
        if (current->first_attribute("texwidth"))
            ShadyUtil::readX(current, "texwidth", frame->asAnimation(), &ShadyCore::Frame::Animation::setTexWidth);
        if (current->first_attribute("texheight"))
            ShadyUtil::readX(current, "texheight", frame->asAnimation(), &ShadyCore::Frame::Animation::setTexHeight);
        if (current->first_attribute("xoffset"))
            ShadyUtil::readX(current, "xoffset", frame->asAnimation(), &ShadyCore::Frame::Animation::setOffsetX);
        if (current->first_attribute("yoffset"))
            ShadyUtil::readX(current, "yoffset", frame->asAnimation(), &ShadyCore::Frame::Animation::setOffsetY);
        if (current->first_attribute("duration"))
            ShadyUtil::readX(current, "duration", frame->asAnimation(), &ShadyCore::Frame::Animation::setDuration);
        if (current->first_attribute("rendergroup"))
            ShadyUtil::readX(current, "rendergroup", frame->asAnimation(), &ShadyCore::Frame::Animation::setRenderGroup);

        if (frame->asAnimation().hasBlendOptions()) {
            data->current = current->first_node("blend");
            processBlend(data, &frame->asAnimation());
        }

    } else {
        if (current->first_attribute("unknown"))
            ShadyUtil::readX(current, "unknown", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setUnknown, true);
        if (current->first_attribute("xtexoffset"))
            ShadyUtil::readX(current, "xtexoffset", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setTexOffsetX);
        if (current->first_attribute("ytexoffset"))
            ShadyUtil::readX(current, "ytexoffset", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setTexOffsetY);
        if (current->first_attribute("texwidth"))
            ShadyUtil::readX(current, "texwidth", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setTexWidth);
        if (current->first_attribute("texheight"))
            ShadyUtil::readX(current, "texheight", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setTexHeight);
        if (current->first_attribute("xoffset"))
            ShadyUtil::readX(current, "xoffset", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setOffsetX);
        if (current->first_attribute("yoffset"))
            ShadyUtil::readX(current, "yoffset", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setOffsetY);
        if (current->first_attribute("duration"))
            ShadyUtil::readX(current, "duration", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setDuration);
        if (current->first_attribute("rendergroup"))
            ShadyUtil::readX(current, "rendergroup", (ShadyCore::Frame::Animation&)frame->asMove(), &ShadyCore::Frame::Animation::setRenderGroup);

        if (frame->asMove().hasBlendOptions()) {
            data->current = current->first_node("blend");
            processBlend(data, (ShadyCore::Frame::Animation*)&frame->asMove());
        }

        data->current = current->first_node("traits");      if (data->current) processTraits(data, &frame->asMove());
        data->current = current->first_node("collision");   if (data->current) processBoxes(data, &frame->asMove().getCollisionBoxes());
        data->current = current->first_node("hit");         if (data->current) processBoxes(data, &frame->asMove().getHitBoxes());
        data->current = current->first_node("attack");      if (data->current) processBoxes(data, &frame->asMove().getAttackBoxes());
        data->current = current->first_node("effect");      if (data->current) processEffect(data, &frame->asMove());
        data->current = current;
    }

    data->current = current;
}

static void processSequence(XmlReaderData* data, ShadyCore::Pattern* pattern) {
    uint32_t id = atoi(data->current->first_attribute("id")->value());

    if (strcmp(data->current->name(), "clone") == 0) {
        eraseExcept(pattern, id, 0);
        ShadyCore::Sequence& seq = pattern->createSequence();
        seq.initialize(id, -1);
        seq.asClone().setTargetId(atoi(data->current->first_attribute("target")->value()));
    } else {
        rapidxml::xml_node<>* current = data->current;

        uint32_t index = atoi(current->first_attribute("index")->value());
        ShadyCore::Sequence* seq = 0;

        uint32_t i; for(i = 0; i < pattern->getSequenceCount(); ++i) {
            if (id != pattern->getSequence(i).getId()) continue;
            uint32_t sindex = pattern->getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_ANIMATION
                ? pattern->getSequence(i).asAnimation().getIndex() : pattern->getSequence(i).asMove().getIndex();
            if (index != sindex) continue;
            seq = &pattern->getSequence(i);
            break;
        }

        if (current->first_attribute("remove")) {
            if (seq) pattern->eraseSequence(i);
            return;
        }

        if (!seq) {
            throw;
            //seq = &pattern->createSequence();
            //seq->initialize(id, index);
        }

        if (current->first_attribute("replace")
            || pattern->getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_CLONE) {
            seq->initialize(id, index);
        }

        if (pattern->getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_ANIMATION) {
            if (current->first_attribute("loop"))
                ShadyUtil::readX(current, "loop", seq->asAnimation(), &ShadyCore::Sequence::Animation::setLoop);
        } else {
            if (current->first_attribute("loop"))
                ShadyUtil::readX(current, "loop", (ShadyCore::Sequence::Animation&)seq->asMove(), &ShadyCore::Sequence::Animation::setLoop);
            if (current->first_attribute("movelock"))
                ShadyUtil::readX(current, "movelock", seq->asMove(), &ShadyCore::Sequence::Move::setMoveLock);
            if (current->first_attribute("actionlock"))
                ShadyUtil::readX(current, "actionlock", seq->asMove(), &ShadyCore::Sequence::Move::setActionLock);
        }

        for (rapidxml::xml_node<>* iter = data->current = current->first_node(); iter; iter = data->current = iter->next_sibling()) {
            processFrame(data, seq->getType() == ShadyCore::Sequence::SEQUENCE_ANIMATION ? &seq->asAnimation() : &seq->asMove());
        } data->current = current;
    }
}

void ShadyCli::MergeCommand::processPattern(ShadyCore::Pattern* pattern, boost::filesystem::path filename) {
    if (!boost::filesystem::is_regular_file(filename)) return; // ignore
    if (filename.extension() != ".xml") return; // ignore

    std::ifstream input(filename.string(), std::ios::in | std::ios::binary);
    XmlReaderData* data = new XmlReaderData(input);
    input.close();
    if (pattern->isAnimationOnly() && strcmp(data->current->name(), "animpattern")
        || !pattern->isAnimationOnly() && strcmp(data->current->name(), "movepattern")) return;

    for (rapidxml::xml_node<>* iter = data->current = data->current->first_node(); iter; iter = data->current = iter->next_sibling()) {
		processSequence(data, pattern);
	}
}