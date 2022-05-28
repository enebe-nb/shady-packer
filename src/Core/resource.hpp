#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace ShadyCore {
	void* Allocate(size_t);
	void Deallocate(void*);

	class Resource {};

	class TextResource : public Resource {
	public:
		char* data = 0;
		uint32_t length;

		inline void destroy() { if (data) Deallocate(data); }
		inline void initialize(uint32_t s) { this->destroy(); data = (char*)Allocate((length = s) + 1); data[length] = '\0'; }
	};

	class LabelResource : public Resource {
	public:
		double begin;
		double end;

		inline void destroy() {}
		inline void initialize() {}
	};

	// TODO reading/convertion
	class Palette : public Resource {
	public:
		uint8_t bitsPerPixel;
		// align 0x3
		uint8_t* data = 0;

		inline void destroy() { if (data) Deallocate(data); }
		inline void initialize(uint8_t b) { bitsPerPixel = b; this->destroy(); data = (uint8_t*)Allocate(getDataSize()); }
		inline uint32_t getDataSize() const { return 256 * (bitsPerPixel <= 16 ? 2 : 4); }
		//inline uint32_t getColor(int index) const { index *= 3; return (index == 0 ? 0x00 : 0xff << 24) + ((uint32_t)data[index] << 16) + ((uint32_t)data[index + 1] << 8) + ((uint32_t)data[index + 2]); }
		//inline void setColor(int index, uint32_t value) { index *= 3; data[index] = value >> 16; data[index + 1] = value >> 8; data[index + 2] = value; }

		static void setTrueColorAvailable(bool value);
		static uint16_t packColor(uint32_t color, bool transparent);
		static inline uint16_t packColor(uint32_t color) { return packColor(color, (color >> 24) < 0x80); }
		static uint32_t unpackColor(uint16_t color);
		void pack();
		void unpack();
	};

	class Image : public Resource {
	public:
		void** vtable;
		uint8_t bitsPerPixel = 0;
		// align 0x3
		uint32_t width, height, paddedWidth;
		uint32_t altSize = 0;
		uint8_t* palette = 0;
		uint8_t* raw = 0;

		inline void destroy() { if (raw) Deallocate(raw); }
		inline void initialize() { this->destroy(); raw = (uint8_t*)Allocate(getRawSize()); }
		inline void initialize(uint8_t b, uint32_t w, uint32_t h, uint32_t p, uint32_t a = 0) {
			bitsPerPixel = b; width = w; height = h; paddedWidth = p; altSize = a;
			this->initialize();
		}

		inline const uint32_t getRawSize() const { return altSize ? altSize : paddedWidth * height * (bitsPerPixel == 24 ? 4 : bitsPerPixel/8); }
	};

	class Sfx : public Resource {
	public:
		uint16_t format = 1;
		uint16_t channels;
		uint32_t sampleRate;
		uint32_t byteRate;
		uint16_t blockAlign;
		uint16_t bitsPerSample;
		uint16_t infoSize = 0;
		// align 2
		uint8_t* data = 0;
		uint32_t size;

		inline void destroy() { if (data) Deallocate(data); }
		inline void initialize(uint32_t s) { this->destroy(); data = (uint8_t*)Allocate(size = s); infoSize = 0; }
	};

	class Schema : public Resource {
	public:
		class Image {
		public:
			const char* const name;
			int32_t x, y, w, h;

			inline Image(const char* name) : name(name) {}
			inline Image(Image&& o) : name(o.name), x(o.x), y(o.y), w(o.w), h(o.h) { (const char*&)o.name = 0; }
			virtual ~Image();
		};

		class Object {
		protected:
			uint32_t id; uint8_t type;
			inline Object(uint32_t id, uint8_t type) : id(id), type(type) {}
		public:
			virtual ~Object() = default;
			inline const uint32_t& getId() const { return id; }
			inline const uint8_t& getType() const { return type; }
		};

		class GuiObject : public Object {
		public:
			uint32_t imageIndex;
			int32_t x, y, mirror;

			inline GuiObject(uint32_t id, uint8_t type) : Object(id, type) {}
		};

		class GuiNumber : public GuiObject {
		public:
			int32_t w, h, fontSpacing, textSpacing, size, floatSize;

			inline GuiNumber(uint32_t id) : GuiObject(id, 6) {}
		};

		class Clone : public Object {
		public:
			uint32_t targetId;

			inline Clone(uint32_t id) : Object(id, 7) {}
		};

		class Sequence : public Object {
		public:
			class BlendOptions {
			public:
				uint16_t mode; // maybe pack
				uint32_t color;
				uint16_t scaleX, scaleY;
				int16_t flipVert, flipHorz, angle;
			};

			class MoveTraits {
			public:
				uint16_t damage, proration, chipDamage, spiritDamage, untech, power, limit;
				uint16_t onHitPlayerStun, onHitEnemyStun, onBlockPlayerStun, onBlockEnemyStun;
				uint16_t onHitCardGain, onBlockCardGain, onAirHitSetSequence, onGroundHitSetSequence;
				int16_t speedX, speedY;
				uint16_t onHitSfx, onHitEffect, attackLevel;

				// TODO tight packed?
				uint8_t comboModifier;
				uint32_t frameFlags, attackFlags;
			};

			class BBox {
			public:
				int32_t left, up, right, down;
				uint8_t unknown = 0;
			};

			class MoveEffect {
			public:
				int32_t pivotX, pivotY;
				int32_t positionXExtra, positionYExtra;
				int32_t positionX, positionY;
				uint16_t unknown02RESETSTATE; //TODO
				int16_t speedX, speedY;
			};

			class Frame {
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
				inline bool hasBlendOptions() const { return renderGroup == 2; }
				virtual ~Frame() = default;
			};

			class MoveFrame : public Frame {
			public:
				MoveTraits traits;
				std::vector<BBox> cBoxes;
				std::vector<BBox> hBoxes;
				std::vector<BBox> aBoxes;
				MoveEffect effect;

				virtual ~MoveFrame() = default;
			};

		public:
			uint16_t moveLock, actionLock;
			bool loop;

			std::vector<Frame*> frames;
			inline Sequence(uint32_t id, bool isAnimation) : Object(id, isAnimation ? 8 : 9) {}
			virtual ~Sequence();
		};

		// --- DATA ---
		std::vector<Image> images;
		std::vector<Object*> objects;

		inline void destroy() { images.clear(); for (auto object : objects) delete object; objects.clear(); }
	};
}
