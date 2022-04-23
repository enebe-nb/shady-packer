#pragma once
#include <cstdint>
#include <vector>
#include <string>

#ifndef ShadyAllocate
#define ShadyAllocate(s) (new uint8_t[s])
//#error ShadyAllocate is not defined.
#endif

#ifndef ShadyDeallocate
#define ShadyDeallocate(p) (delete[] p)
//#error ShadyDeallocate is not defined.
#endif

namespace ShadyCore {
	class Resource {};

	class TextResource : public Resource {
	protected:
		char* data = 0;
		uint32_t length;
	public:
		inline void destroy() { if (data) ShadyDeallocate(data); }
		inline void initialize(uint32_t s) { this->destroy(); data = (char*)ShadyAllocate((length = s) + 1); }

		inline const size_t& getLength() const { return length; }
		inline const char* getData() const { return data; }
		inline char* getData() { return data; }
	};

    class LabelResource : public Resource {
    protected:
        double offset;
        double size;
    public:
        inline const double& getOffset() const { return offset; }
        inline const double& getSize() const { return size; }
        inline void setOffset(double o) { offset = o; }
        inline void setSize(double s) { size = s; }
    };

    class Palette : public Resource {
    protected:
		uint8_t bitsPerPixel;
        uint8_t* data = 0;
    public:
		inline void destroy() { if (data) ShadyDeallocate(data); }
		inline void initialize(uint8_t b) { bitsPerPixel = b; this->destroy(); data = ShadyAllocate(getSize()); }

		inline const uint8_t getBitsPerPixel() const { return bitsPerPixel; }
		inline uint32_t getSize() const { return 256 * (bitsPerPixel <= 16 ? 2 : 4); }
        inline const uint8_t* getData() const { return data; }
        inline uint8_t* getData() { return data; }
		//inline uint32_t getColor(int index) const { index *= 3; return (index == 0 ? 0x00 : 0xff << 24) + ((uint32_t)data[index] << 16) + ((uint32_t)data[index + 1] << 8) + ((uint32_t)data[index + 2]); }
		//inline void setColor(int index, uint32_t value) { index *= 3; data[index] = value >> 16; data[index + 1] = value >> 8; data[index + 2] = value; }

		static uint16_t packColor(uint32_t color, bool = false);
		static uint32_t unpackColor(uint16_t color);
		void pack();
		void unpack();
    };

	class Image : public Resource {
    protected:
		void** vtable;
		uint8_t bitsPerPixel = 0;
        uint32_t width, height, paddedWidth;
		uint32_t altSize = 0;
		uint8_t* palette = 0;
		uint8_t* raw = 0;

	public:
		inline void destroy() { if (raw) ShadyDeallocate(raw); }
		inline void initialize(uint8_t b, uint32_t w, uint32_t h, uint32_t p, uint32_t a = 0) {
			bitsPerPixel = b; width = w; height = h; paddedWidth = p; altSize = a;
			this->destroy();
			raw = ShadyAllocate(getRawSize());
		}

        inline const uint8_t& getBitsPerPixel() const { return bitsPerPixel; }
		inline const uint32_t& getWidth() const { return width; }
        inline const uint32_t& getHeight() const { return height; }
        inline const uint32_t& getPaddedWidth() const { return paddedWidth; } // TODO implicit pad creation;
        inline const uint32_t& getAltSize() const { return altSize; }
		inline const uint32_t& getRawSize() const { return altSize ? altSize : paddedWidth * height * (bitsPerPixel == 8 ? 1 : 4); }
        inline const uint8_t* getRawImage() const { return raw; }
		inline uint8_t* getRawImage() { return raw; }
		inline uint8_t*& getPalette() { return palette; }
	};

	class Sfx : public Resource {
	protected:
		uint16_t format = 1;
		uint16_t channels;
		uint32_t sampleRate;
		uint32_t byteRate;
		uint16_t blockAlign;
		uint16_t bitsPerSample;
		uint16_t infoSize;
		// align 2
		uint8_t* data = 0;
		uint32_t size;
	public:
		inline void destroy() { if (data) ShadyDeallocate(data); }
		inline void initialize(uint32_t s) { this->destroy(); data = ShadyAllocate(size = s); }

		inline const uint16_t& getChannels() const { return channels; }
		inline const uint32_t& getSampleRate() const { return sampleRate; }
		inline const uint32_t& getByteRate() const { return byteRate; }
		inline const uint16_t& getBlockAlign() const { return blockAlign; }
		inline const uint16_t& getBitsPerSample() const { return bitsPerSample; }
		inline void setChannels(uint16_t value) { channels = value; }
		inline void setSampleRate(uint32_t value) { sampleRate = value; }
		inline void setByteRate(uint32_t value) { byteRate = value; }
		inline void setBlockAlign(uint16_t value) { blockAlign = value; }
		inline void setBitsPerSample(uint16_t value) { bitsPerSample = value; }

		inline const uint32_t& getSize() const { return size; }
		inline const uint8_t* getData() const { return data; }
		inline uint8_t* getData() { return data; }
	};

	class Schema : public Resource {
	public:
		enum Type {UNKNOWN, LAYOUT, ANIMATION, MOVEDATA};
		class Destructible { public: virtual ~Destructible() {}; }; // TODO
		class Image : public Destructible {
		public:
			const std::string name;
			int32_t x, y, w, h;
		public:
			inline Image(const std::string_view& name) : name(name) {}
			//virtual ~Image();

			inline const char* getName() const { return name.data(); }
			inline const int32_t& getX() const { return x; }
			inline const int32_t& getY() const { return y; }
			inline const int32_t& getWidth() const { return w; }
			inline const int32_t& getHeight() const { return h; }
			inline void setX(int32_t value) { x = value; }
			inline void setY(int32_t value) { y = value; }
			inline void setWidth(int32_t value) { w = value; }
			inline void setHeight(int32_t value) { h = value; }
			inline char* getDataAddr() { return (char*)&x; }
		};

		class Object : public Destructible {
		protected:
			uint32_t id; uint8_t type;
			inline Object(uint32_t id, uint8_t type) : id(id), type(type) {}
		public:
			inline const uint32_t& getId() const { return id; }
			inline const uint8_t& getType() const { return type; }
		};

		class GuiObject : public Object {
		public:
			uint32_t imageIndex;
			int32_t x, y, mirror;
		public:
			inline GuiObject(uint32_t id, uint8_t type) : Object(id, type) {}

			inline const uint32_t& getImageIndex() const { return imageIndex; }
			inline const int32_t& getX() const { return x; }
			inline const int32_t& getY() const { return y; }
			inline const int32_t& getMirror() const { return mirror; }
			inline void setImageIndex(uint32_t value) { imageIndex = value; }
			inline void setX(int32_t value) { x = value; }
			inline void setY(int32_t value) { y = value; }
			inline void setMirror(int32_t value) { mirror = value; }
			inline char* getDataAddr() { return (char*)&imageIndex; }
		};

		class GuiNumber : public GuiObject {
		public:
			int32_t w, h, fontSpacing, textSpacing, size, floatSize;
		public:
			inline GuiNumber(uint32_t id) : GuiObject(id, 6) {}

			inline const int32_t& getWidth() const { return w; }
			inline const int32_t& getHeight() const { return h; }
			inline const int32_t& getFontSpacing() const { return fontSpacing; }
			inline const int32_t& getTextSpacing() const { return textSpacing; }
			inline const int32_t& getSize() const { return size; }
			inline const int32_t& getFloatSize() const { return floatSize; }
			inline void setWidth(const int32_t value) { w = value; }
			inline void setHeight(const int32_t value) { h = value; }
			inline void setFontSpacing(const int32_t value) { fontSpacing = value; }
			inline void setTextSpacing(const int32_t value) { textSpacing = value; }
			inline void setSize(const int32_t value) { size = value; }
			inline void setFloatSize(const int32_t value) { floatSize = value; }
			inline char* getDataAddr() { return (char*)&w; }
		};

		class Clone : public Object {
		protected:
			uint32_t targetId;
		public:
			inline Clone(uint32_t id) : Object(id, 7) {}

			inline const uint32_t& getTargetId() const { return targetId; }
			inline void setTargetId(const uint32_t value) { targetId = value; }
		};

		class Sequence : public Object {
		public:
			class BlendOptions : public Destructible {
			public:
				// TODO check ordering
				uint32_t color; // offset 0x02
				uint16_t mode, scaleX, scaleY;
				int16_t flipVert, flipHorz, angle;
			public:
				inline const uint16_t& getMode() const { return mode; }
				inline const uint32_t& getColor() const { return color; }
				inline const uint16_t& getScaleX() const { return scaleX; }
				inline const uint16_t& getScaleY() const { return scaleY; }
				inline const int16_t& getFlipVert() const { return flipVert; }
				inline const int16_t& getFlipHorz() const { return flipHorz; }
				inline const int16_t& getAngle() const { return angle; }
				inline void setMode(const uint16_t value) { mode = value; }
				inline void setColor(const uint32_t value) { color = value; }
				inline void setScaleX(const uint16_t value) { scaleX = value; }
				inline void setScaleY(const uint16_t value) { scaleY = value; }
				inline void setFlipVert(const int16_t value) { flipVert = value; }
				inline void setFlipHorz(const int16_t value) { flipHorz = value; }
				inline void setAngle(const int16_t value) { angle = value; }
			};

			class MoveTraits : public Destructible {
			public:
				uint16_t damage, proration, chipDamage, spiritDamage, untech, power, limit;
				uint16_t onHitPlayerStun, onHitEnemyStun, onBlockPlayerStun, onBlockEnemyStun;
				uint16_t onHitCardGain, onBlockCardGain, onAirHitSetSequence, onGroundHitSetSequence;
				int16_t speedX, speedY;
				uint16_t onHitSfx, onHitEffect, attackLevel;

				// the following is tight packed
				uint8_t comboModifier; // offset 0x28 (40)
				uint32_t frameFlags, attackFlags;
			public:
				inline char* getDataAddr() { return (char*)&damage; }

				inline const uint16_t& getDamage() const { return damage; }
				inline const uint16_t& getProration() const { return proration; }
				inline const uint16_t& getChipDamage() const { return chipDamage; }
				inline const uint16_t& getSpiritDamage() const { return spiritDamage; }
				inline const uint16_t& getUntech() const { return untech; }
				inline const uint16_t& getPower() const { return power; }
				inline const uint16_t& getLimit() const { return limit; }
				inline const uint16_t& getOnHitPlayerStun() const { return onHitPlayerStun; }
				inline const uint16_t& getOnHitEnemyStun() const { return onHitEnemyStun; }
				inline const uint16_t& getOnBlockPlayerStun() const { return onBlockPlayerStun; }
				inline const uint16_t& getOnBlockEnemyStun() const { return onBlockEnemyStun; }
				inline const uint16_t& getOnHitCardGain() const { return onHitCardGain; }
				inline const uint16_t& getOnBlockCardGain() const { return onBlockCardGain; }
				inline const uint16_t& getOnAirHitSetSequence() const { return onAirHitSetSequence; }
				inline const uint16_t& getOnGroundHitSetSequence() const { return onGroundHitSetSequence; }
				inline const int16_t& getSpeedX() const { return speedX; }
				inline const int16_t& getSpeedY() const { return speedY; }
				inline const uint16_t& getOnHitSfx() const { return onHitSfx; }
				inline const uint16_t& getOnHitEffect() const { return onHitEffect; }
				inline const uint16_t& getAttackLevel() const { return attackLevel; }
				inline const uint8_t& getComboModifier() const { return comboModifier; }
				inline const uint32_t& getFrameFlags() const { return frameFlags; }
				inline const uint32_t& getAttackFlags() const { return attackFlags; }
				inline void setDamage(const uint16_t value) { damage = value; }
				inline void setProration(const uint16_t value) { proration = value; }
				inline void setChipDamage(const uint16_t value) { chipDamage = value; }
				inline void setSpiritDamage(const uint16_t value) { spiritDamage = value; }
				inline void setUntech(const uint16_t value) { untech = value; }
				inline void setPower(const uint16_t value) { power = value; }
				inline void setLimit(const uint16_t value) { limit = value; }
				inline void setOnHitPlayerStun(const uint16_t value) { onHitPlayerStun = value; }
				inline void setOnHitEnemyStun(const uint16_t value) { onHitEnemyStun = value; }
				inline void setOnBlockPlayerStun(const uint16_t value) { onBlockPlayerStun = value; }
				inline void setOnBlockEnemyStun(const uint16_t value) { onBlockEnemyStun = value; }
				inline void setOnHitCardGain(const uint16_t value) { onHitCardGain = value; }
				inline void setOnBlockCardGain(const uint16_t value) { onBlockCardGain = value; }
				inline void setOnAirHitSetSequence(const uint16_t value) { onAirHitSetSequence = value; }
				inline void setOnGroundHitSetSequence(const uint16_t value) { onGroundHitSetSequence = value; }
				inline void setSpeedX(const int16_t value) { speedX = value; }
				inline void setSpeedY(const int16_t value) { speedY = value; }
				inline void setOnHitSfx(const uint16_t value) { onHitSfx = value; }
				inline void setOnHitEffect(const uint16_t value) { onHitEffect = value; }
				inline void setAttackLevel(const uint16_t value) { attackLevel = value; }
				inline void setComboModifier(const uint8_t value) { comboModifier = value; }
				inline void setFrameFlags(const uint32_t value) { frameFlags = value; }
				inline void setAttackFlags(const uint32_t value) { attackFlags = value; }

				static const char* const comboModifierNames[8];
				static const char* const frameFlagNames[32];
				static const char* const attackFlagNames[32];
			};

			class BBoxList : public Destructible {
			public:
				class BBox {
				public:
					int32_t left, up, right, down;
					uint8_t unknown = 0;
				public:
					inline const int32_t& getLeft() const { return left; }
					inline const int32_t& getUp() const { return up; }
					inline const int32_t& getRight() const { return right; }
					inline const int32_t& getDown() const { return down; }
					inline const uint8_t& getUnknown() const { return unknown; }
					inline void setLeft(const int32_t value) { left = value; }
					inline void setUp(const int32_t value) { up = value; }
					inline void setRight(const int32_t value) { right = value; }
					inline void setDown(const int32_t value) { down = value; }
					inline void setUnknown(const uint8_t value) { unknown = value; }
					inline char* getDataAddr() { return (char*)&left; }
				};
			protected:
				std::vector<BBox> boxes;
				bool attack;
			public:
				inline BBoxList(bool attack) : attack(attack) {}

				inline bool isAttack() const { return attack; }
				inline const uint32_t getBoxCount() const { return boxes.size(); }
				inline const BBox& getBox(int i) const { return boxes[i]; }
				inline BBox& getBox(int i) { return boxes[i]; }
				// TODO remake vector management
				inline BBox& createBox() { boxes.emplace_back(); return boxes.back(); }
				inline void eraseBox(uint32_t index) { boxes.erase(boxes.begin() + index); }
			};

			class MoveEffect : public Destructible {
			public:
				int32_t pivotX, pivotY;
				int32_t positionXExtra, positionYExtra;
				int32_t positionX, positionY;
				uint16_t unknown02RESETSTATE; //TODO
				int16_t speedX, speedY;
			public:
				inline const int32_t& getPivotX() const { return pivotX; }
				inline const int32_t& getPivotY() const { return pivotY; }
				inline const int32_t& getPositionXExtra() const { return positionXExtra; }
				inline const int32_t& getPositionYExtra() const { return positionYExtra; }
				inline const int32_t& getPositionX() const { return positionX; }
				inline const int32_t& getPositionY() const { return positionY; }
				inline const uint16_t& getUnknown02() const { return unknown02RESETSTATE; }
				inline const int16_t& getSpeedX() const { return speedX; }
				inline const int16_t& getSpeedY() const { return speedY; }
				inline void setPivotX(const int32_t value) { pivotX = value; }
				inline void setPivotY(const int32_t value) { pivotY = value; }
				inline void setPositionXExtra(const int32_t value) { positionXExtra = value; }
				inline void setPositionYExtra(const int32_t value) { positionYExtra = value; }
				inline void setPositionX(const int32_t value) { positionX = value; }
				inline void setPositionY(const int32_t value) { positionY = value; }
				inline void setUnknown02(const uint16_t value) { unknown02RESETSTATE = value; }
				inline void setSpeedX(const int16_t value) { speedX = value; }
				inline void setSpeedY(const int16_t value) { speedY = value; }
				inline char* getDataAddr() { return (char*)&pivotX; }
			};

			class Frame : public Destructible {
			public:
				uint16_t imageIndex;
				uint16_t unknown;
				uint16_t texOffsetX;
				uint16_t texOffsetY;
				uint16_t texWidth;
				uint16_t texHeight;
				int16_t offsetX;
				int16_t offsetY;
				uint16_t duration;
				uint8_t renderGroup;

				BlendOptions blendOptions;
			public:
				inline const uint16_t& getImageIndex() const { return imageIndex; }
				inline const uint16_t& getUnknown() const { return unknown; } // @Unknown &raw[2]; 2Bytes
				inline const uint16_t& getTexOffsetX() const { return texOffsetX; }
				inline const uint16_t& getTexOffsetY() const { return texOffsetY; }
				inline const uint16_t& getTexWidth() const { return texWidth; }
				inline const uint16_t& getTexHeight() const { return texHeight; }
				inline const int16_t& getOffsetX() const { return offsetX; }
				inline const int16_t& getOffsetY() const { return offsetY; }
				inline const uint16_t& getDuration() const { return duration; }
				inline const uint8_t& getRenderGroup() const { return renderGroup; }
				inline void setImageIndex(uint16_t value) { imageIndex = value; }
				inline void setUnknown(const uint16_t value) { unknown = value; } // @Unknown &raw[2]; 2Bytes
				inline void setTexOffsetX(const uint16_t value) { texOffsetX = value; }
				inline void setTexOffsetY(const uint16_t value) { texOffsetY = value; }
				inline void setTexWidth(const uint16_t value) { texWidth = value; }
				inline void setTexHeight(const uint16_t value) { texHeight = value; }
				inline void setOffsetX(const int16_t value) { offsetX = value; }
				inline void setOffsetY(const int16_t value) { offsetY = value; }
				inline void setDuration(const uint16_t value) { duration = value; }
				inline void setRenderGroup(const uint8_t value) { renderGroup = value; }
				inline char* getDataAddr() { return (char*)&imageIndex; }

				inline const BlendOptions& getBlendOptions() const { if (hasBlendOptions()) return blendOptions; throw; }
				inline BlendOptions& getBlendOptions() { if (hasBlendOptions()) return blendOptions; throw; }
				inline bool hasBlendOptions() const { return renderGroup == 2; }
			};

			class MoveFrame : public Frame {
			protected:
				MoveTraits traits;
				BBoxList cBoxes;
				BBoxList hBoxes;
				BBoxList aBoxes;
				MoveEffect effect;
			public:
				inline MoveFrame() : cBoxes(false), hBoxes(false), aBoxes(true) {}

				inline const MoveTraits& getTraits() const { return traits; }
				inline MoveTraits& getTraits() { return traits; }
				inline const BBoxList& getCollisionBoxes() const { return cBoxes; }
				inline BBoxList& getCollisionBoxes() { return cBoxes; }
				inline const BBoxList& getHitBoxes() const { return hBoxes; }
				inline BBoxList& getHitBoxes() { return hBoxes; }
				inline const BBoxList& getAttackBoxes() const { return aBoxes; }
				inline BBoxList& getAttackBoxes() { return aBoxes; }
				inline const MoveEffect& getEffect() const { return effect; }
				inline MoveEffect& getEffect() { return effect; }
			};

		protected:
			uint32_t index;
		public:
			uint16_t moveLock, actionLock;
			bool loop;

			std::vector<Frame*> frames; // TODO better encapsulation
			inline Sequence(uint32_t id, bool isAnimation) : Object(id, isAnimation ? 8 : 9) {}

			inline const uint16_t& getMoveLock() const { return moveLock; }
			inline const uint16_t& getActionLock() const { return actionLock; }
			inline const bool& hasLoop() const { return loop; }
			inline void setMoveLock(const uint16_t value) { moveLock = value; }
			inline void setActionLock(const uint16_t value) { actionLock = value; }
			inline void setLoop(const bool value) { loop = value; }
			inline char* getDataAddr() { return (char*)&moveLock; }
		};

		// --- DATA ---
		std::vector<Image> images;
		std::vector<Object*> objects;
		//std::map<int, Object*> objectsById;

		inline void destroy() { images.clear(); }
	};
}
