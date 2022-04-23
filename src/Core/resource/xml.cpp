#include <rapidxml.hpp>
#include "../resource.hpp"
#include "readerwriter.hpp"
#include "../util/xmlprinter.hpp"
#include <map>
#include <charconv>
#include <unordered_map>

namespace {
	struct XmlNode {
		enum {
			STATIC, MUTABLE,
			SLIDERHORZ, SLIDERVERT,
			UNUSED04, UNUSED05,
			NUMBER,

			CLONE, ANIMATION, MOVE,

			BLEND, TRAITS, EFFECT,
			COLLISION, HIT, ATTACK
		};

		static constexpr size_t fnv1a(const char* s, std::size_t count) {
			size_t val = std::_FNV_offset_basis;
			for (size_t i = 0; i < count; ++i) {
				val ^= static_cast<size_t>(s[i]);
				val *= std::_FNV_prime;
			} return val;
		}
		static constexpr size_t fnv1a(const std::string_view& s) { return fnv1a(s.data(), s.size()); }

		const size_t hash;
		const char* const name;
		constexpr XmlNode(const char* name) : hash(fnv1a(name)), name(name) {}
	};

	constexpr XmlNode NodeList[] = {
		"static", "mutable",
		"sliderhorz", "slidervert",
		"unused04", "unused05",
		"number",

		"clone", "animation", "move",

		"blend", "traits", "effect",
		"collision", "hit", "attack"
	};

	template <typename T, int base = 10>
	static inline void setter(T& var, const char* value) {
		size_t len = strlen(value);
		std::from_chars(value, value + len, var, base);
	}

	using setter_t = void (*)(void*, const char*);
	using AttributeMap = std::unordered_map<std::string, std::pair<setter_t, size_t> >;
	static inline void readAttributes(rapidxml::xml_node<>* node, char* addr, const AttributeMap& mapping) {
		for (auto attr = node->first_attribute(); attr; attr = attr->next_attribute()) {
			auto iter = mapping.find(attr->name());
			if (iter == mapping.end()) continue;
			iter->second.first(addr + iter->second.second, attr->value());
		}
	}

	const AttributeMap XmlImage = {
		{"xposition",   {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, x)}},
		{"xpos",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, x)}},
		{"x",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, x)}},
		{"yposition",   {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, y)}},
		{"ypos",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, y)}},
		{"y",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, y)}},
		{"width",       {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, w)}},
		{"w",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, w)}},
		{"height",      {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, h)}},
		{"h",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Image, h)}},
	};

	const AttributeMap XmlGuiObject = {
		{"xposition",   {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, x)}},
		{"xpos",        {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, x)}},
		{"x",           {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, x)}},
		{"yposition",   {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, y)}},
		{"ypos",        {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, y)}},
		{"y",           {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, y)}},
		{"mirror",      {(setter_t)setter<int32_t>,    offsetof(ShadyCore::Schema::GuiObject, mirror)}},
	};

	const AttributeMap XmlGuiNumber = {
		{"xposition",   {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, x)}},
		{"xpos",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, x)}},
		{"x",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, x)}},
		{"yposition",   {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, y)}},
		{"ypos",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, y)}},
		{"y",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, y)}},
		{"width",       {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, w)}},
		{"w",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, w)}},
		{"height",      {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, h)}},
		{"h",           {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, h)}},
		{"fontspacing", {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, fontSpacing)}},
		{"textspacing", {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, textSpacing)}},
		{"size",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, size)}},
		{"floatsize",   {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::GuiNumber, floatSize)}},
	};

	const AttributeMap XmlSequenceMove = {
		{"loop",        {(setter_t)setter<uint8_t>,     offsetof(ShadyCore::Schema::Sequence, loop)}},
		{"mlock",       {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence, moveLock)}},
		{"movelock",    {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence, moveLock)}},
		{"alock",       {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence, actionLock)}},
		{"actionlock",  {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence, actionLock)}},
	};

	const AttributeMap XmlFrame = {
		{"unknown",     {(setter_t)setter<uint16_t, 16>,offsetof(ShadyCore::Schema::Sequence::Frame, unknown)}}, // TODO check
		{"xtexoffset",  {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texOffsetX)}},
		{"xtex",        {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texOffsetX)}},
		{"ytexoffset",  {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texOffsetY)}},
		{"ytex",        {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texOffsetY)}},
		{"texwidth",    {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texWidth)}},
		{"wtex",        {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texWidth)}},
		{"texheight",   {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texHeight)}},
		{"htex",        {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, texHeight)}},
		{"xoffset",     {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::Frame, offsetX)}},
		{"x",           {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::Frame, offsetX)}},
		{"yoffset",     {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::Frame, offsetY)}},
		{"y",           {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::Frame, offsetY)}},
		{"duration",    {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::Frame, duration)}},
		{"rendergroup", {(setter_t)setter<uint8_t>,     offsetof(ShadyCore::Schema::Sequence::Frame, renderGroup)}},
		{"render",      {(setter_t)setter<uint8_t>,     offsetof(ShadyCore::Schema::Sequence::Frame, renderGroup)}},
	};

	const AttributeMap XmlBlendOptions = {
		{"color",       {(setter_t)setter<uint32_t,16>, offsetof(ShadyCore::Schema::Sequence::BlendOptions, color)}},
		{"mode",        {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::BlendOptions, mode)}},
		{"xscale",      {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::BlendOptions, scaleX)}},
		{"yscale",      {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::BlendOptions, scaleY)}},
		{"vertflip",    {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::BlendOptions, flipVert)}},
		{"horzflip",    {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::BlendOptions, flipHorz)}},
		{"angle",       {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::BlendOptions, angle)}},
	};

	constexpr XmlNode FlagList[] = {
		// frame flags
		"standing",             "crouching",          "airborne",           "down",
		"guardcancel",          "cancellable",        "takecounterhit",     "superarmor",
		"normalarmor",          "fflag10GUARD_POINT", "grazing",            "fflag12GUARDING",
		"grabinvul",            "melleinvul",         "bulletinvul",        "airattackinvul",
		"highattackinvul",      "lowattackinvul",     "objectattackinvul",  "fflag20REFLECT_BULLET",
		"fflag21FLIP_VELOCITY", "movimentcancel",     "fflag23AFTER_IMAGE", "fflag24LOOP_ANIMATION",
		"fflag25ATTACK_OBJECT", "fflag26",            "fflag27",            "fflag28",
		"fflag29",              "fflag30",            "fflag31",            "fflag32",

		// attack flags
		"aflag01STAGGER",         "highrightblock",       "lowrightblock",      "airblockable",
		"unblockable",            "ignorearmor",          "hitonwrongblock",    "aflag08GRAB",
		"aflag09",                "aflag10",              "inducecounterhit",   "skillattacktype",
		"spellattacktype",        "airattacktype",        "highattacktype",     "lowattacktype",
		"objectattacktype",       "aflag18UNREFLECTABLE", "aflag19",            "guardcrush",
		"friendlyfire",           "aflag22stagger",       "isbullet",           "ungrazeble",
		"aflag25DRAINS_ON_GRAZE", "aflag26",              "aflag27",            "aflag28",
		"aflag29",                "aflag30",              "aflag31",            "aflag32",

		// modifiers
		"liftmodifier",  "smashmodifier",   "bordermodifier", "chainmodifier",
		"spellmodifier", "countermodifier", "modifier07",     "modifier08",
};

	const AttributeMap XmlMoveTraits = {
		{"damage",      {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, damage)}},
		{"proration",   {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, proration)}},
		{"chipdamage",  {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, chipDamage)}},
		{"spiritdamage",{(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, spiritDamage)}},
		{"untech",      {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, untech)}},
		{"power",       {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, power)}},
		{"limit",       {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, limit)}},

		{"onhitplayerstun",         {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onHitPlayerStun)}},
		{"onhitenemystun",          {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onHitEnemyStun)}},
		{"onblockplayerstun",       {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onBlockPlayerStun)}},
		{"onblockenemystun",        {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onBlockEnemyStun)}},
		{"onhitcardgain",           {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onHitCardGain)}},
		{"onblockcardgain",         {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onBlockCardGain)}},
		{"onairhitsetsequence",     {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onAirHitSetSequence)}},
		{"ongroundhitsetsequence",  {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onGroundHitSetSequence)}},

		{"xspeed",      {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::MoveTraits, speedX)}},
		{"yspeed",      {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::MoveTraits, speedY)}},
		{"onhitsfx",    {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onHitSfx)}},
		{"onhiteffect", {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, onHitEffect)}},
		{"attacklevel", {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveTraits, attackLevel)}},
		{"modifiers",   {(setter_t)setter<uint8_t, 16>, offsetof(ShadyCore::Schema::Sequence::MoveTraits, comboModifier)}},
		{"frameflags",  {(setter_t)setter<uint32_t, 16>,offsetof(ShadyCore::Schema::Sequence::MoveTraits, frameFlags)}},
		{"attackflags", {(setter_t)setter<uint32_t, 16>,offsetof(ShadyCore::Schema::Sequence::MoveTraits, attackFlags)}},
	};

	const AttributeMap XmlMoveEffect = {
		{"xpivot",          {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, pivotX)}},
		{"ypivot",          {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, pivotY)}},
		{"xpositionextra",  {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, positionXExtra)}},
		{"ypositionextra",  {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, positionYExtra)}},
		{"xposition",       {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, positionX)}},
		{"yposition",       {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, positionY)}},
		{"unknown02",       {(setter_t)setter<uint16_t>,    offsetof(ShadyCore::Schema::Sequence::MoveEffect, unknown02RESETSTATE)}},
		{"xspeed",          {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, speedX)}},
		{"yspeed",          {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, speedY)}},
	};

	const AttributeMap XmlMoveBox = {
		{"left",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBoxList::BBox, left)}},
		{"up",          {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBoxList::BBox, up)}},
		{"right",       {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBoxList::BBox, right)}},
		{"down",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBoxList::BBox, down)}},
		{"unknown",     {(setter_t)setter<uint8_t>,     offsetof(ShadyCore::Schema::Sequence::BBoxList::BBox, unknown)}},
	};
}

static inline int addImage(ShadyCore::Schema& resource, rapidxml::xml_attribute<>* attr) {
	using namespace rapidxml;
	if (!attr) return -1;

	const char* name = attr->value();
	for (int i = 0; i < resource.images.size(); ++i) {
		if (strcmp(resource.images[i].getName(), name) == 0) return i;
	}

	auto& image = resource.images.emplace_back(name);
	return resource.images.size() - 1;
}

static inline int addImage(ShadyCore::Schema& resource, rapidxml::xml_node<>* imageNode) {
	using namespace rapidxml;
	if (!imageNode) return -1;

	int id = addImage(resource, imageNode->first_attribute("name"));
	if (id >= 0) readAttributes(imageNode, (char*)&resource.images[id], XmlImage);

	return id;
}

static int findType(const char* name) {
	const size_t hash = XmlNode::fnv1a(name);
	for (int i = 0; i < std::extent<decltype(NodeList)>::value; ++i) {
		if (hash == NodeList[i].hash) return i;
	} return -1;
}

static void readerSchemaXmlNode(void* data, rapidxml::xml_node<>* node) {
	const auto schema = reinterpret_cast<ShadyCore::Schema*>(data);
	const size_t type = findType(node->name());

	switch (type) {
		case XmlNode::STATIC:
		case XmlNode::SLIDERHORZ:
		case XmlNode::SLIDERVERT:
		case XmlNode::UNUSED04:
		case XmlNode::UNUSED05: {
			auto attr = node->first_attribute("id");
			int id = attr ? atoi(attr->value()) : 0;
			auto obj = new ShadyCore::Schema::GuiObject(id, type);
			schema->objects.push_back(obj);
			obj->setImageIndex(addImage(*schema, node->first_node("image")));
			readAttributes(node, (char*)obj, XmlGuiObject);
		} break;
		case XmlNode::MUTABLE: {
			auto attr = node->first_attribute("id");
			int id = attr ? atoi(attr->value()) : 0;
			auto obj = new ShadyCore::Schema::GuiObject(id, type);
			schema->objects.push_back(obj);
			readAttributes(node, (char*)obj, XmlGuiObject);
		} break;
		case XmlNode::NUMBER: {
			auto attr = node->first_attribute("id");
			int id = attr ? atoi(attr->value()) : 0;
			auto obj = new ShadyCore::Schema::GuiNumber(id);
			schema->objects.push_back(obj);
			obj->setImageIndex(addImage(*schema, node->first_node("image")));
			readAttributes(node, (char*)obj, XmlGuiNumber);
		} break;
		case XmlNode::CLONE: {
			auto attr = node->first_attribute("id");
			int id = attr ? atoi(attr->value()) : 0;
			auto obj = new ShadyCore::Schema::Clone(id);
			schema->objects.push_back(obj);
			attr = node->first_attribute("target");
			if (attr) obj->setTargetId(atoi(attr->value()));
		} break;
		case XmlNode::ANIMATION:
		case XmlNode::MOVE: {
			auto attr = node->first_attribute("id");
			int id = attr ? atoi(attr->value()) : 0;
			auto obj = new ShadyCore::Schema::Sequence(id, type == 8);
			schema->objects.push_back(obj);
			readAttributes(node, (char*)obj, XmlSequenceMove);
			for (auto iter = node->first_node(); iter; iter = iter->next_sibling()) {
				auto frame = type == XmlNode::ANIMATION ? new ShadyCore::Schema::Sequence::Frame() : new ShadyCore::Schema::Sequence::MoveFrame();
				obj->frames.push_back(frame);
				frame->setImageIndex(addImage(*schema, iter->first_attribute("image")));
				readAttributes(iter, (char*)frame, XmlFrame);
				for (auto prop = iter->first_node(); prop; prop = prop->next_sibling()) readerSchemaXmlNode(frame, prop);
			}
		} break;
		case XmlNode::BLEND: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::Frame*>(data);
			readAttributes(node, (char*)&frame->blendOptions, XmlBlendOptions); break;
		} break;
		case XmlNode::TRAITS: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data);
			auto& traits = frame->getTraits();
			traits.frameFlags = traits.attackFlags = traits.comboModifier = 0;
			readAttributes(node, (char*)&traits, XmlMoveTraits);
			for (auto flag = node->first_node(); flag; flag = flag->next_sibling()) {
				size_t hash = XmlNode::fnv1a(flag->name());
				// TODO *slow*, maybe do order the list
				for (auto i = 0; i < 72; ++i) {
					if (FlagList[i].hash == hash) {
						if (i < 32) {
							traits.frameFlags |= 1 << i;
						} else if (i < 64) {
							i -= 32; traits.attackFlags |= 1 << i;
						} else {
							i -= 64; traits.comboModifier |= 1 << i;
						}
					}
				}
			}
		} break;
		case XmlNode::EFFECT: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data);
			readAttributes(node, (char*)&frame->getEffect(), XmlMoveEffect);
		} break;
		case XmlNode::COLLISION: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data);
			for(auto box = node->first_node("box"); box; box = box->next_sibling("box")) {
				auto& object = frame->getCollisionBoxes().createBox();
				readAttributes(box, (char*)&object, XmlMoveBox);
			}
		} break;
		case XmlNode::HIT: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data);
			for(auto box = node->first_node("box"); box; box = box->next_sibling("box")) {
				auto& object = frame->getHitBoxes().createBox();
				readAttributes(box, (char*)&object, XmlMoveBox);
			}
		} break;
		case XmlNode::ATTACK: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data);
			for(auto box = node->first_node("box"); box; box = box->next_sibling("box")) {
				auto& object = frame->getAttackBoxes().createBox();
				readAttributes(box, (char*)&object, XmlMoveBox);
			}
		} break;
	}
}

namespace _private {
void readerSchemaXml(ShadyCore::Schema& resource, std::istream& input) {
	using namespace rapidxml;
	xml_document<> document;

	input.seekg(0, std::ios::end);
	size_t len = input.tellg();
	input.seekg(0);

	char* data = document.allocate_string(0, len + 1);
	data[len] = '\0';
	input.read(data, len);
	document.parse<0>(data);

	auto root = document.first_node();
	if (!root) return;
	for (auto iter = root->first_node(); iter; iter = iter->next_sibling()) {
		readerSchemaXmlNode(&resource, iter);
	}
}
}

static inline void printImageNode(ShadyCore::Schema::Image& image, ShadyUtil::XmlPrinter& printer) {
	printer.openNode("image");
	printer.appendAttribute("name", image.getName());
	printer.appendAttribute("xposition", image.getX());
	printer.appendAttribute("yposition", image.getY());
	printer.appendAttribute("width", image.getWidth());
	printer.appendAttribute("height", image.getHeight());
	printer.closeNode();
}

static void writerSchemaXmlNode(ShadyCore::Schema& resource, ShadyUtil::XmlPrinter& printer, void* data, size_t type, int index = 0) {
	printer.openNode(NodeList[type].name);
	switch (type) {
		case XmlNode::STATIC:
		case XmlNode::SLIDERHORZ:
		case XmlNode::SLIDERVERT:
		case XmlNode::UNUSED04:
		case XmlNode::UNUSED05: {
			auto object = reinterpret_cast<ShadyCore::Schema::GuiObject*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("xposition", object->x);
			printer.appendAttribute("yposition", object->y);
			printer.appendAttribute("mirror", object->mirror);
			printImageNode(resource.images[object->imageIndex], printer);
		} break;
		case XmlNode::MUTABLE: {
			auto object = reinterpret_cast<ShadyCore::Schema::GuiObject*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("xposition", object->x);
			printer.appendAttribute("yposition", object->y);
		} break;
		case XmlNode::NUMBER: {
			auto object = reinterpret_cast<ShadyCore::Schema::GuiNumber*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("xposition", object->x);
			printer.appendAttribute("yposition", object->y);
			printer.appendAttribute("width", object->w);
			printer.appendAttribute("height", object->h);
			printer.appendAttribute("fontspacing", object->fontSpacing);
			printer.appendAttribute("textspacing", object->textSpacing);
			printer.appendAttribute("size", object->size);
			printer.appendAttribute("floatsize", object->floatSize);
			printImageNode(resource.images[object->imageIndex], printer);
		} break;
		case XmlNode::CLONE: {
			auto object = reinterpret_cast<ShadyCore::Schema::Clone*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("target", object->getTargetId());
		} break;
		case XmlNode::ANIMATION: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("index", index);
			printer.appendAttribute("loop", object->loop);

			for (uint32_t i = 0; i < object->frames.size(); ++i) {
				auto frame = object->frames[i];
				printer.openNode("frame");
				printer.appendAttribute("image", resource.images[frame->imageIndex].name.c_str());
				printer.appendAttribute("index", i);

				printer.appendAttribute("unknown", frame->getUnknown(), true);
				printer.appendAttribute("xtexoffset", frame->getTexOffsetX());
				printer.appendAttribute("ytexoffset", frame->getTexOffsetY());
				printer.appendAttribute("texwidth", frame->getTexWidth());
				printer.appendAttribute("texheight", frame->getTexHeight());
				printer.appendAttribute("xoffset", frame->getOffsetX());
				printer.appendAttribute("yoffset", frame->getOffsetY());
				printer.appendAttribute("duration", frame->getDuration());
				printer.appendAttribute("rendergroup", frame->getRenderGroup());

				if (frame->hasBlendOptions()) writerSchemaXmlNode(resource, printer, &frame->blendOptions, XmlNode::BLEND);

				printer.closeNode();
			}
		} break;
		case XmlNode::MOVE: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("index", index);
			printer.appendAttribute("loop", object->loop);
			printer.appendAttribute("movelock", object->getMoveLock());
			printer.appendAttribute("actionlock", object->getActionLock());

			for (uint32_t i = 0; i < ((ShadyCore::Schema::Sequence*)object)->frames.size(); ++i) {
				auto frame = (ShadyCore::Schema::Sequence::MoveFrame*)object->frames[i];
				printer.openNode("frame");
				printer.appendAttribute("image", resource.images[frame->imageIndex].name.c_str());
				printer.appendAttribute("index", i);

				printer.appendAttribute("unknown", frame->getUnknown(), true);
				printer.appendAttribute("xtexoffset", frame->getTexOffsetX());
				printer.appendAttribute("ytexoffset", frame->getTexOffsetY());
				printer.appendAttribute("texwidth", frame->getTexWidth());
				printer.appendAttribute("texheight", frame->getTexHeight());
				printer.appendAttribute("xoffset", frame->getOffsetX());
				printer.appendAttribute("yoffset", frame->getOffsetY());
				printer.appendAttribute("duration", frame->getDuration());
				printer.appendAttribute("rendergroup", frame->getRenderGroup());

				if (frame->hasBlendOptions()) writerSchemaXmlNode(resource, printer, &frame->blendOptions, XmlNode::BLEND);

				writerSchemaXmlNode(resource, printer, &frame->getTraits(), XmlNode::TRAITS);
				if (frame->getCollisionBoxes().getBoxCount()) writerSchemaXmlNode(resource, printer, &frame->getCollisionBoxes(), XmlNode::COLLISION);
				if (frame->getHitBoxes().getBoxCount()) writerSchemaXmlNode(resource, printer, &frame->getHitBoxes(), XmlNode::HIT);
				if (frame->getAttackBoxes().getBoxCount()) writerSchemaXmlNode(resource, printer, &frame->getAttackBoxes(), XmlNode::ATTACK);
				writerSchemaXmlNode(resource, printer, &frame->getEffect(), XmlNode::EFFECT);

				printer.closeNode();
			}
		} break;
		case XmlNode::BLEND: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::BlendOptions*>(data);
			printer.appendAttribute("mode", object->getMode());
			printer.appendAttribute("color", object->getColor(), true);
			printer.appendAttribute("xscale", object->getScaleX());
			printer.appendAttribute("yscale", object->getScaleY());
			printer.appendAttribute("vertflip", object->getFlipVert());
			printer.appendAttribute("horzflip", object->getFlipHorz());
			printer.appendAttribute("angle", object->getAngle());
		} break;
		case XmlNode::TRAITS: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::MoveTraits*>(data);
			printer.appendAttribute("damage", object->getDamage());
			printer.appendAttribute("proration", object->getProration());
			printer.appendAttribute("chipdamage", object->getChipDamage());
			printer.appendAttribute("spiritdamage", object->getSpiritDamage());
			printer.appendAttribute("untech", object->getUntech());
			printer.appendAttribute("power", object->getPower());
			printer.appendAttribute("limit", object->getLimit());
			printer.appendAttribute("onhitplayerstun", object->getOnHitPlayerStun());
			printer.appendAttribute("onhitenemystun", object->getOnHitEnemyStun());
			printer.appendAttribute("onblockplayerstun", object->getOnBlockPlayerStun());
			printer.appendAttribute("onblockenemystun", object->getOnBlockEnemyStun());
			printer.appendAttribute("onhitcardgain", object->getOnHitCardGain());
			printer.appendAttribute("onblockcardgain", object->getOnBlockCardGain());
			printer.appendAttribute("onairhitsetsequence", object->getOnAirHitSetSequence());
			printer.appendAttribute("ongroundhitsetsequence", object->getOnGroundHitSetSequence());
			printer.appendAttribute("xspeed", object->getSpeedX());
			printer.appendAttribute("yspeed", object->getSpeedY());
			printer.appendAttribute("onhitsfx", object->getOnHitSfx());
			printer.appendAttribute("onhiteffect", object->getOnHitEffect());
			printer.appendAttribute("attacklevel", object->getAttackLevel());

			for (int i = 0; i < 32; ++i) {
				if (i < 8 && (object->getComboModifier() >> i & 1)) {
					printer.openNode(FlagList[i + 64].name);
					printer.closeNode();
				} if (object->getFrameFlags() >> i & 1) {
					printer.openNode(FlagList[i].name);
					printer.closeNode();
				} if (object->getAttackFlags() >> i & 1) {
					printer.openNode(FlagList[i + 32].name);
					printer.closeNode();
				}
			}
		} break;
		case XmlNode::EFFECT: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::MoveEffect*>(data);
			printer.appendAttribute("xpivot", object->getPivotX());
			printer.appendAttribute("ypivot", object->getPivotY());
			printer.appendAttribute("xpositionextra", object->getPositionXExtra());
			printer.appendAttribute("ypositionextra", object->getPositionYExtra());
			printer.appendAttribute("xposition", object->getPositionX());
			printer.appendAttribute("yposition", object->getPositionY());
			printer.appendAttribute("unknown02", object->getUnknown02(), true);
			printer.appendAttribute("xspeed", object->getSpeedX());
			printer.appendAttribute("yspeed", object->getSpeedY());
		} break;
		case XmlNode::COLLISION:
		case XmlNode::HIT:
		case XmlNode::ATTACK: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::BBoxList*>(data);
			for (int i = 0; i < object->getBoxCount(); ++i) {
				auto& box = object->getBox(i);
				printer.openNode("box");
				printer.appendAttribute("left", box.getLeft());
				printer.appendAttribute("up", box.getUp());
				printer.appendAttribute("right", box.getRight());
				printer.appendAttribute("down", box.getDown());
				if (type == XmlNode::ATTACK)
					printer.appendAttribute("unknown", box.getUnknown());
				printer.closeNode();
			}
		} break;
	}
	printer.closeNode();
}

namespace _private {
void writerSchemaXml(ShadyCore::Schema& resource, std::ostream& output) {
	ShadyUtil::XmlPrinter printer(output);
	if (!resource.objects.size()) return;
	auto type = resource.objects[0]->getType();
	if (type <= 6) {
		printer.openNode("layout");
		printer.appendAttribute("version", "4");
	} else {
		if (type == 9)  printer.openNode("movepattern");
		else printer.openNode("animpattern");
		printer.appendAttribute("version", "5");
	}

	int lastId = 0, lastIndex = -1;
	for (auto object : resource.objects) {
		writerSchemaXmlNode(resource, printer, object, object->getType(), object->getId() == lastId ? ++lastIndex : (lastIndex = 0));
		lastId = object->getId();
	}

	printer.closeNode();
}
}