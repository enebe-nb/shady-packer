#pragma once

#include "../Core/resource.hpp"

#include <nuklear.h>

namespace ShadyGui {
	class ImageRenderer {
	protected:
		ShadyCore::Image* image = 0;
		ShadyCore::Palette* palette = 0;
		struct nk_image texture = nk_image_id(0);
		int32_t x = 0, y = 0;
		float scale = 1;

		void reloadTexture();
	public:
		ImageRenderer();
		virtual ~ImageRenderer();
		inline int32_t getPositionX() const { return x; }
		inline int32_t getPositionY() const { return y; }
		inline float getScale() const { return scale; }
		inline ShadyCore::Image* getImage() const { return image; }
		inline ShadyCore::Palette* getPalette() const { return palette; }
		inline void setPosition(uint32_t x, uint32_t y) { this->x = x; this->y = y; }
		inline void setScale(float s) { scale = s; }
		void setImage(ShadyCore::Image*);
		void setPalette(ShadyCore::Palette*);
		void draw(nk_context*);
	};

	class AudioRenderer {
	protected:
		int length;
		uint8_t* buffer = 0;
	public:
		AudioRenderer();
		virtual ~AudioRenderer();
		void setAudio(ShadyCore::Sfx*);
		bool play();
	};
}
