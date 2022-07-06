#include <rapidxml.hpp>
#include "../resource.hpp"
#include "readerwriter.hpp"
#include "../util/xmlprinter.hpp"
#include <map>
#include <cstring>
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
		#if defined(_WIN64)
			static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
			const size_t fnv_offset_basis = 14695981039346656037ULL;
			const size_t fnv_prime = 1099511628211ULL;
		#else /* defined(_WIN64) */
			static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
			const size_t fnv_offset_basis = 2166136261U;
			const size_t fnv_prime = 16777619U;
		#endif /* defined(_WIN64) */
			size_t val = fnv_offset_basis;
			for (size_t i = 0; i < count; ++i) {
				val ^= static_cast<size_t>(s[i]);
				val *= fnv_prime;
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
		{"unknown",     {(setter_t)setter<uint16_t, 16>,offsetof(ShadyCore::Schema::Sequence::Frame, unknown)}},
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

	constexpr const char* FlagList[] = {
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

	const std::unordered_map<std::string_view, std::pair<int, int>> FlagMap = {
		// frame flags
		{"standing",             {0, 0x00000001}}, {"crouching",          {0, 0x00000002}}, {"airborne",           {0, 0x00000004}}, {"down",                  {0, 0x00000008}},
		{"guardcancel",          {0, 0x00000010}}, {"cancellable",        {0, 0x00000020}}, {"takecounterhit",     {0, 0x00000040}}, {"superarmor",            {0, 0x00000080}},
		{"normalarmor",          {0, 0x00000100}}, {"fflag10GUARD_POINT", {0, 0x00000200}}, {"grazing",            {0, 0x00000400}}, {"fflag12GUARDING",       {0, 0x00000800}},
		{"grabinvul",            {0, 0x00001000}}, {"melleinvul",         {0, 0x00002000}}, {"bulletinvul",        {0, 0x00004000}}, {"airattackinvul",        {0, 0x00008000}},
		{"highattackinvul",      {0, 0x00010000}}, {"lowattackinvul",     {0, 0x00020000}}, {"objectattackinvul",  {0, 0x00040000}}, {"fflag20REFLECT_BULLET", {0, 0x00080000}},
		{"fflag21FLIP_VELOCITY", {0, 0x00100000}}, {"movimentcancel",     {0, 0x00200000}}, {"fflag23AFTER_IMAGE", {0, 0x00400000}}, {"fflag24LOOP_ANIMATION", {0, 0x00800000}},
		{"fflag25ATTACK_OBJECT", {0, 0x01000000}}, {"fflag26",            {0, 0x02000000}}, {"fflag27",            {0, 0x04000000}}, {"fflag28",               {0, 0x08000000}},
		{"fflag29",              {0, 0x10000000}}, {"fflag30",            {0, 0x20000000}}, {"fflag31",            {0, 0x40000000}}, {"fflag32",               {0, 0x80000000}},

		// attack flags
		{"aflag01STAGGER",         {1, 0x00000001}}, {"highrightblock",       {1, 0x00000002}}, {"lowrightblock",      {1, 0x00000004}}, {"airblockable",    {1, 0x00000008}},
		{"unblockable",            {1, 0x00000010}}, {"ignorearmor",          {1, 0x00000020}}, {"hitonwrongblock",    {1, 0x00000040}}, {"aflag08GRAB",     {1, 0x00000080}},
		{"aflag09",                {1, 0x00000100}}, {"aflag10",              {1, 0x00000200}}, {"inducecounterhit",   {1, 0x00000400}}, {"skillattacktype", {1, 0x00000800}},
		{"spellattacktype",        {1, 0x00001000}}, {"airattacktype",        {1, 0x00002000}}, {"highattacktype",     {1, 0x00004000}}, {"lowattacktype",   {1, 0x00008000}},
		{"objectattacktype",       {1, 0x00010000}}, {"aflag18UNREFLECTABLE", {1, 0x00020000}}, {"aflag19",            {1, 0x00040000}}, {"guardcrush",      {1, 0x00080000}},
		{"friendlyfire",           {1, 0x00100000}}, {"aflag22stagger",       {1, 0x00200000}}, {"isbullet",           {1, 0x00400000}}, {"ungrazeble",      {1, 0x00800000}},
		{"aflag25DRAINS_ON_GRAZE", {1, 0x01000000}}, {"aflag26",              {1, 0x02000000}}, {"aflag27",            {1, 0x04000000}}, {"aflag28",         {1, 0x08000000}},
		{"aflag29",                {1, 0x10000000}}, {"aflag30",              {1, 0x20000000}}, {"aflag31",            {1, 0x40000000}}, {"aflag32",         {1, 0x80000000}},

		// modifiers
		{"liftmodifier",  {2, 0x01}}, {"smashmodifier",   {2, 0x02}}, {"bordermodifier", {2, 0x04}}, {"chainmodifier", {2, 0x08}},
		{"spellmodifier", {2, 0x10}}, {"countermodifier", {2, 0x20}}, {"modifier07",     {2, 0x40}}, {"modifier08",    {2, 0x80}},
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
		{"unknown02",       {(setter_t)setter<uint16_t, 16>,offsetof(ShadyCore::Schema::Sequence::MoveEffect, unknown02RESETSTATE)}},
		{"xspeed",          {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, speedX)}},
		{"yspeed",          {(setter_t)setter<int16_t>,     offsetof(ShadyCore::Schema::Sequence::MoveEffect, speedY)}},
	};

	const AttributeMap XmlMoveBox = {
		{"left",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBox, left)}},
		{"up",          {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBox, up)}},
		{"right",       {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBox, right)}},
		{"down",        {(setter_t)setter<int32_t>,     offsetof(ShadyCore::Schema::Sequence::BBox, down)}},
	};
}

// TODO hashing?
static inline int addImage(ShadyCore::Schema& resource, rapidxml::xml_attribute<>* attr) {
	if (!attr) return -1;

	const char* name = attr->value();
	for (int i = 0; i < resource.images.size(); ++i) {
		if (strcmp(resource.images[i].name, name) == 0) return i;
	}

	char* buffer = new char[attr->value_size() + 1];
	memcpy(buffer, name, attr->value_size());
	buffer[attr->value_size()] = '\0';
	auto& image = resource.images.emplace_back(buffer);
	return resource.images.size() - 1;
}

static inline int addImage(ShadyCore::Schema& resource, rapidxml::xml_node<>* imageNode) {
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
			obj->imageIndex = addImage(*schema, node->first_node("image"));
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
			obj->imageIndex = addImage(*schema, node->first_node("image"));
			readAttributes(node, (char*)obj, XmlGuiNumber);
		} break;
		case XmlNode::CLONE: {
			auto attr = node->first_attribute("id");
			int id = attr ? atoi(attr->value()) : 0;
			auto obj = new ShadyCore::Schema::Clone(id);
			schema->objects.push_back(obj);
			attr = node->first_attribute("target");
			if (attr) obj->targetId = atoi(attr->value());
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
				frame->imageIndex = addImage(*schema, iter->first_attribute("image"));
				readAttributes(iter, (char*)frame, XmlFrame);
				for (auto prop = iter->first_node(); prop; prop = prop->next_sibling()) readerSchemaXmlNode(frame, prop);
			}
		} break;
		case XmlNode::BLEND: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::Frame*>(data);
			readAttributes(node, (char*)&frame->blendOptions, XmlBlendOptions); break;
		} break;
		case XmlNode::TRAITS: {
			auto& traits = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data)->traits;
			traits.frameFlags = traits.attackFlags = traits.comboModifier = 0;
			readAttributes(node, (char*)&traits, XmlMoveTraits);
			for (auto flag = node->first_node(); flag; flag = flag->next_sibling()) {
				auto i = FlagMap.find(flag->name());
				if (i != FlagMap.end()) switch (i->second.first) {
					case 0: traits.frameFlags |= i->second.second; break;
					case 1: traits.attackFlags |= i->second.second; break;
					case 2: traits.comboModifier |= i->second.second; break;
				}
			}
		} break;
		case XmlNode::EFFECT: {
			const auto frame = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data);
			readAttributes(node, (char*)&frame->effect, XmlMoveEffect);
		} break;
		case XmlNode::COLLISION: {
			auto& boxes = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data)->cBoxes;
			for(auto box = node->first_node("box"); box; box = box->next_sibling("box")) {
				readAttributes(box, (char*)&boxes.emplace_back(), XmlMoveBox);
			}
		} break;
		case XmlNode::HIT: {
			auto& boxes = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data)->hBoxes;
			for(auto box = node->first_node("box"); box; box = box->next_sibling("box")) {
				readAttributes(box, (char*)&boxes.emplace_back(), XmlMoveBox);
			}
		} break;
		case XmlNode::ATTACK: {
			auto& boxes = reinterpret_cast<ShadyCore::Schema::Sequence::MoveFrame*>(data)->aBoxes;
			for(auto box = node->first_node("box"); box; box = box->next_sibling("box")) {
				auto& abox = boxes.emplace_back();
				readAttributes(box, (char*)&abox, XmlMoveBox);
				auto extra = box->first_node("box");
				if (extra) {
					abox.extra = new ShadyCore::Schema::Sequence::BBox;
					readAttributes(extra, (char*)abox.extra, XmlMoveBox);
				}
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
	printer.appendAttribute("name", image.name);
	printer.appendAttribute("xposition", image.x);
	printer.appendAttribute("yposition", image.y);
	printer.appendAttribute("width", image.w);
	printer.appendAttribute("height", image.h);
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
			printer.appendAttribute("target", object->targetId);
		} break;
		case XmlNode::ANIMATION: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("index", index);
			printer.appendAttribute("loop", object->loop);

			for (uint32_t i = 0; i < object->frames.size(); ++i) {
				auto frame = object->frames[i];
				printer.openNode("frame");
				printer.appendAttribute("image", resource.images[frame->imageIndex].name);
				printer.appendAttribute("index", i);

				printer.appendAttribute("unknown", frame->unknown, true);
				printer.appendAttribute("xtexoffset", frame->texOffsetX);
				printer.appendAttribute("ytexoffset", frame->texOffsetY);
				printer.appendAttribute("texwidth", frame->texWidth);
				printer.appendAttribute("texheight", frame->texHeight);
				printer.appendAttribute("xoffset", frame->offsetX);
				printer.appendAttribute("yoffset", frame->offsetY);
				printer.appendAttribute("duration", frame->duration);
				printer.appendAttribute("rendergroup", frame->renderGroup);

				if (frame->hasBlendOptions()) writerSchemaXmlNode(resource, printer, &frame->blendOptions, XmlNode::BLEND);

				printer.closeNode();
			}
		} break;
		case XmlNode::MOVE: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence*>(data);
			printer.appendAttribute("id", object->getId());
			printer.appendAttribute("index", index);
			printer.appendAttribute("loop", object->loop);
			printer.appendAttribute("movelock", object->moveLock);
			printer.appendAttribute("actionlock", object->actionLock);

			for (uint32_t i = 0; i < ((ShadyCore::Schema::Sequence*)object)->frames.size(); ++i) {
				auto frame = (ShadyCore::Schema::Sequence::MoveFrame*)object->frames[i];
				printer.openNode("frame");
				printer.appendAttribute("image", resource.images[frame->imageIndex].name);
				printer.appendAttribute("index", i);

				printer.appendAttribute("unknown", frame->unknown, true);
				printer.appendAttribute("xtexoffset", frame->texOffsetX);
				printer.appendAttribute("ytexoffset", frame->texOffsetY);
				printer.appendAttribute("texwidth", frame->texWidth);
				printer.appendAttribute("texheight", frame->texHeight);
				printer.appendAttribute("xoffset", frame->offsetX);
				printer.appendAttribute("yoffset", frame->offsetY);
				printer.appendAttribute("duration", frame->duration);
				printer.appendAttribute("rendergroup", frame->renderGroup);

				if (frame->hasBlendOptions()) writerSchemaXmlNode(resource, printer, &frame->blendOptions, XmlNode::BLEND);

				writerSchemaXmlNode(resource, printer, &frame->traits, XmlNode::TRAITS);
				if (frame->cBoxes.size()) writerSchemaXmlNode(resource, printer, &frame->cBoxes, XmlNode::COLLISION);
				if (frame->hBoxes.size()) writerSchemaXmlNode(resource, printer, &frame->hBoxes, XmlNode::HIT);
				if (frame->aBoxes.size()) writerSchemaXmlNode(resource, printer, &frame->aBoxes, XmlNode::ATTACK);
				writerSchemaXmlNode(resource, printer, &frame->effect, XmlNode::EFFECT);

				printer.closeNode();
			}
		} break;
		case XmlNode::BLEND: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::BlendOptions*>(data);
			printer.appendAttribute("mode", object->mode);
			printer.appendAttribute("color", object->color, true);
			printer.appendAttribute("xscale", object->scaleX);
			printer.appendAttribute("yscale", object->scaleY);
			printer.appendAttribute("vertflip", object->flipVert);
			printer.appendAttribute("horzflip", object->flipHorz);
			printer.appendAttribute("angle", object->angle);
		} break;
		case XmlNode::TRAITS: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::MoveTraits*>(data);
			printer.appendAttribute("damage", object->damage);
			printer.appendAttribute("proration", object->proration);
			printer.appendAttribute("chipdamage", object->chipDamage);
			printer.appendAttribute("spiritdamage", object->spiritDamage);
			printer.appendAttribute("untech", object->untech);
			printer.appendAttribute("power", object->power);
			printer.appendAttribute("limit", object->limit);
			printer.appendAttribute("onhitplayerstun", object->onHitPlayerStun);
			printer.appendAttribute("onhitenemystun", object->onHitEnemyStun);
			printer.appendAttribute("onblockplayerstun", object->onBlockPlayerStun);
			printer.appendAttribute("onblockenemystun", object->onBlockEnemyStun);
			printer.appendAttribute("onhitcardgain", object->onHitCardGain);
			printer.appendAttribute("onblockcardgain", object->onBlockCardGain);
			printer.appendAttribute("onairhitsetsequence", object->onAirHitSetSequence);
			printer.appendAttribute("ongroundhitsetsequence", object->onGroundHitSetSequence);
			printer.appendAttribute("xspeed", object->speedX);
			printer.appendAttribute("yspeed", object->speedY);
			printer.appendAttribute("onhitsfx", object->onHitSfx);
			printer.appendAttribute("onhiteffect", object->onHitEffect);
			printer.appendAttribute("attacklevel", object->attackLevel);

			for (int i = 0; i < 32; ++i) {
				if (i < 8 && (object->comboModifier >> i & 1)) {
					printer.openNode(FlagList[i + 64]);
					printer.closeNode();
				} if (object->frameFlags >> i & 1) {
					printer.openNode(FlagList[i]);
					printer.closeNode();
				} if (object->attackFlags >> i & 1) {
					printer.openNode(FlagList[i + 32]);
					printer.closeNode();
				}
			}
		} break;
		case XmlNode::EFFECT: {
			auto object = reinterpret_cast<ShadyCore::Schema::Sequence::MoveEffect*>(data);
			printer.appendAttribute("xpivot", object->pivotX);
			printer.appendAttribute("ypivot", object->pivotY);
			printer.appendAttribute("xpositionextra", object->positionXExtra);
			printer.appendAttribute("ypositionextra", object->positionYExtra);
			printer.appendAttribute("xposition", object->positionX);
			printer.appendAttribute("yposition", object->positionY);
			printer.appendAttribute("unknown02", object->unknown02RESETSTATE, true);
			printer.appendAttribute("xspeed", object->speedX);
			printer.appendAttribute("yspeed", object->speedY);
		} break;
		case XmlNode::COLLISION:
		case XmlNode::HIT:
		case XmlNode::ATTACK: {
			auto object = reinterpret_cast<std::vector<ShadyCore::Schema::Sequence::BBox>*>(data);
			for (int i = 0; i < object->size(); ++i) {
				auto& box = object->at(i);
				printer.openNode("box");
				printer.appendAttribute("left", box.left);
				printer.appendAttribute("up", box.up);
				printer.appendAttribute("right", box.right);
				printer.appendAttribute("down", box.down);
				if (type == XmlNode::ATTACK && box.extra) {
					printer.openNode("box");
					printer.appendAttribute("left", box.extra->left);
					printer.appendAttribute("up", box.extra->up);
					printer.appendAttribute("right", box.extra->right);
					printer.appendAttribute("down", box.extra->down);
					printer.closeNode();
				}
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