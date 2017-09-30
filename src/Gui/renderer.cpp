#include "renderer.hpp"

#include <algorithm>
#include <SDL2/SDL_opengl.h>
#include <SDL_audio.h>

ShadyGui::ImageRenderer::ImageRenderer() {
	glGenTextures(1, (GLuint*)&texture.handle.id);
	glBindTexture(GL_TEXTURE_2D, texture.handle.id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
}

ShadyGui::ImageRenderer::~ImageRenderer() {
	glDeleteTextures(1, (GLuint*)&texture.handle.id);
}

void ShadyGui::ImageRenderer::reloadTexture() {
	glBindTexture(GL_TEXTURE_2D, texture.handle.id);

	if (image->getBitsPerPixel() == 8) {
		int size = image->getWidth() * image->getHeight();
		uint8_t* data = new uint8_t[size * 4];
		for (int i = 0; i < size; ++i) {
			memcpy(data + i * 4, palette->getData() + (image->getRawImage()[i] * 3), 3);
			data[i * 4 + 3] = image->getRawImage()[i] ? 255 : 0;
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->getWidth(), image->getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		delete[] data;
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->getWidth(), image->getHeight(), 0, GL_BGRA, GL_UNSIGNED_BYTE, image->getRawImage());
	}

	texture.w = image->getWidth(); texture.h = image->getHeight();
}

void ShadyGui::ImageRenderer::setImage(ShadyCore::Image* i) {
	image = i;
	if (image->getBitsPerPixel() != 8 || palette) reloadTexture();
}

void ShadyGui::ImageRenderer::setPalette(ShadyCore::Palette* p) {
	palette = p;
	if (image && image->getBitsPerPixel() == 8) reloadTexture();
}

void ShadyGui::ImageRenderer::draw(nk_context* context) {
	static const struct nk_color white = { 255, 255, 255, 255 };
	
	nk_command_buffer* canvas = &context->current->buffer;
	struct nk_rect bounds;
	if (!nk_widget(&bounds, context)) return;

	texture.region[0] = std::max(0, -x);
	texture.region[1] = std::max(0, -y);
	texture.region[2] = std::min((float)image->getWidth() - texture.region[0], bounds.w - std::max(0, x));
	texture.region[3] = std::min((float)image->getHeight() - texture.region[1], bounds.h - std::max(0, y));

	bounds.x += std::max(0, x); bounds.y += std::max(0, y);
	bounds.w = texture.region[2]; bounds.h = texture.region[3];
	nk_draw_image(canvas, bounds, &texture, white);
}

static uint32_t aRendererCount = 0;
static SDL_AudioDeviceID aDevice = 0;
static SDL_AudioSpec aTargetSpec = { 0 };

ShadyGui::AudioRenderer::AudioRenderer() {
	if (!aDevice) {
		aTargetSpec.channels = 2;
		aTargetSpec.format = AUDIO_S16;
		aTargetSpec.freq = 44100;
		aTargetSpec.samples = 4092;
		aDevice = SDL_OpenAudioDevice(0, false, &aTargetSpec, &aTargetSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (!aDevice) throw; // TODO better handling
	}

	++aRendererCount;
}

ShadyGui::AudioRenderer::~AudioRenderer() {
	if (buffer) delete[] buffer;
	if (!--aRendererCount) {
		SDL_CloseAudioDevice(aDevice);
		aDevice = 0;
	}
}

void ShadyGui::AudioRenderer::setAudio(ShadyCore::Sfx* audio) {
	if (buffer) delete[] buffer;

	SDL_AudioFormat format;
	switch (audio->getBitsPerSample()) {
		case 4: format = AUDIO_S16; break;
		case 8: format = AUDIO_U8; break;
		case 16: format = AUDIO_S16; break;
		case 32: format = AUDIO_S32; break;
		default: throw;
	}

	SDL_AudioCVT converter;
	SDL_BuildAudioCVT(&converter, format, audio->getChannels(), audio->getSampleRate(), aTargetSpec.format, aTargetSpec.channels, aTargetSpec.freq);
	if (converter.needed) {
		converter.len = audio->getSize();
		buffer = converter.buf = new uint8_t[converter.len * converter.len_mult];
		memcpy(buffer, audio->getData(), converter.len);
		if (SDL_ConvertAudio(&converter)) throw; // TODO better handling
		length = converter.len_cvt;
	} else {
		length = audio->getSize();
		buffer = new uint8_t[length];
		memcpy(buffer, audio->getData(), length);
	}
}

bool ShadyGui::AudioRenderer::play() {
	if (SDL_GetQueuedAudioSize(aDevice)) return false;

	SDL_QueueAudio(aDevice, buffer, length);
	SDL_PauseAudioDevice(aDevice, false);

	return true;
}