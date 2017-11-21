#include "renderer.hpp"

#include <cstring>
#include <epoxy/gl.h>

static RtAudio rtAudio;

ShadyGui::AudioRenderer::AudioRenderer() {
    dispatcher.connect(sigc::mem_fun(*this, &AudioRenderer::stopAudio));
    audioOptions.flags = RTAUDIO_NONINTERLEAVED;
}

ShadyGui::AudioRenderer::~AudioRenderer() {
    stopAudio();
}

void ShadyGui::AudioRenderer::setAudio(ShadyCore::Sfx* audio) {
    stopAudio();

    this->audio = audio;
    params.deviceId = rtAudio.getDefaultOutputDevice();
    params.nChannels = audio->getChannels();
    params.firstChannel = 0;
    frameSize = audio->getChannels() * audio->getBitsPerSample() / 8;

#ifndef NDEBUG
    printf("Audio Bits: %d\n", audio->getBitsPerSample());
#endif
    switch (audio->getBitsPerSample()) {
        case 8: audioFormat = RTAUDIO_SINT8; break;
        case 16: audioFormat = RTAUDIO_SINT16; break;
        case 24: audioFormat = RTAUDIO_SINT24; break;
        case 32: audioFormat = RTAUDIO_SINT32; break;
        default: throw;
    }
}

bool ShadyGui::AudioRenderer::playAudio() {
    if (!audio) return false;
    if (rtAudio.isStreamOpen()) return false;

    try {
        unsigned int frames = 2048;
        rtAudio.openStream(&params, 0, audioFormat, audio->getSampleRate(), &frames, streamCallback, this, &audioOptions);
        rtAudio.startStream();
    } catch(RtAudioError e) {
#ifndef NDEBUG
        e.printMessage();
#endif
        if (rtAudio.isStreamOpen()) rtAudio.closeStream();
    }
}

void ShadyGui::AudioRenderer::stopAudio() {
    if (rtAudio.isStreamOpen()) {
        rtAudio.closeStream();
    } current = 0;
}

int ShadyGui::AudioRenderer::streamCallback(void* output, void* input, unsigned int size, double time, RtAudioStreamStatus status, void* userData) {
    AudioRenderer* self = (AudioRenderer*) userData;
    size *= self->frameSize;

    if (status) printf("Something Wrong on Audio\n");

    uint32_t left = self->audio->getSize() - self->current;
    if (size >= left) {
        memcpy(output, self->audio->getData() + self->current, left);
        memset((uint8_t*)output + left, 0, size - left);
        self->current = self->audio->getSize();
        self->dispatcher.emit();
        return 1;
    }

    memcpy(output, self->audio->getData() + self->current, size);
    self->current += size;
    return 0;
}

ShadyGui::SpriteWidget::SpriteWidget() : Gtk::DrawingArea() {
    add_events(Gdk::BUTTON_PRESS_MASK);
    add_events(Gdk::POINTER_MOTION_MASK);
    add_events(Gdk::SCROLL_MASK);
    get_style_context()->add_class(GTK_STYLE_CLASS_FRAME);
}

ShadyGui::SpriteWidget::~SpriteWidget() { if(surfaceData) delete[] surfaceData; }

bool ShadyGui::SpriteWidget::on_draw(const Cairo::RefPtr<Cairo::Context>& context) {
	Gtk::Allocation allocation = get_allocation();
	int width = allocation.get_width();
	int height = allocation.get_height();

	context->save();
	context->translate(
        (width - (int)image->getWidth()) / 2 + tx,
        (height - (int)image->getHeight()) / 2 + ty);
	context->scale(scale, scale);
	context->set_source(surface);
	context->rectangle(0, 0, image->getWidth(), image->getHeight());
	context->clip();
	context->paint();
	context->restore();

    auto style = get_style_context();
    style->render_frame(context, 0, 0, width, height);
}

// TODO better drag
bool ShadyGui::SpriteWidget::on_button_press_event(GdkEventButton* event) {
    if (event->button != 1) return Gtk::DrawingArea::on_button_press_event(event);

	mx = event->x;
	my = event->y;
    return true;
}

bool ShadyGui::SpriteWidget::on_motion_notify_event(GdkEventMotion* event) {
    int x, y; Gdk::ModifierType state;
    get_window()->get_pointer(x, y, state);
    if (state & GDK_BUTTON1_MASK) {
        tx += event->x - mx;
        ty += event->y - my;
        mx = event->x;
        my = event->y;
    } else return Gtk::DrawingArea::on_motion_notify_event(event);

    queue_draw();
    return true;
}

bool ShadyGui::SpriteWidget::on_scroll_event(GdkEventScroll* event) {
    if (event->direction == GDK_SCROLL_UP) {
        scale *= 2;
    } else if (event->direction == GDK_SCROLL_DOWN) {
        scale /= 2;
    } else return Gtk::DrawingArea::on_scroll_event(event);

    queue_draw();
    return true;
}

static inline uint8_t* buildImageData(ShadyCore::Image* image, ShadyCore::Palette* palette) {
    if (!image) return 0;
    if (image->getBitsPerPixel() != 8) {
        return image->getRawImage();
    } else if (palette) {
		int len = image->getWidth() * image->getHeight();
		uint8_t* data = new uint8_t[len * 4];
		for (int i = 0; i < len; ++i) {
			uint32_t color = palette->getColor(image->getRawImage()[i]);
			memcpy(data + i * 4, &color, 4);
		}
        return data;
    } else return 0;
}

void ShadyGui::SpriteWidget::buildImageData() {
	if (!image || image->getBitsPerPixel() == 8 && !palette) return;
	if (surfaceData) delete[] surfaceData;
	
	uint8_t* rawData = image->getRawImage();
	int width = image->getWidth(), height = image->getHeight();
	int stride = Cairo::ImageSurface::format_stride_for_width(Cairo::FORMAT_ARGB32, width);
	surfaceData = new uint8_t[stride * height];
	if (image->getBitsPerPixel() == 8) {
		int len = width * height;
		for (int i = 0; i < len; ++i) {
			uint32_t color = palette->getColor(rawData[i]);
			memcpy(surfaceData + ((i / width) * stride + i % width * 4), &color, 4);
		}
	} else {
		int len = width * height;
		for (int i = 0; i < len; ++i) {
			uint32_t color = *(uint32_t*)(rawData + i * 4);
			memcpy(surfaceData + ((i / width) * stride + i % width * 4), &color, 4);
		}
	}
	
    surface = Cairo::SurfacePattern::create(Cairo::ImageSurface::create(surfaceData, Cairo::FORMAT_ARGB32, image->getWidth(), image->getHeight(), stride));
    surface->set_filter(Cairo::FILTER_NEAREST);
    queue_draw();
}

void ShadyGui::SpriteWidget::setImage(ShadyCore::Image* image) {
	if (!image) return;
	this->image = image;
    buildImageData();
}

void ShadyGui::SpriteWidget::setPalette(ShadyCore::Palette* palette) {
	if (!palette) return;
	this->palette = palette;
	buildImageData();
}

ShadyGui::PaletteWidget::PaletteWidget(Gtk::Window* window, ShadyCore::Palette* palette)
	: Gtk::DrawingArea(), palette(palette), dialog("Select a color") {
    get_style_context()->add_class(GTK_STYLE_CLASS_FRAME);
    padding = get_style_context()->get_padding(Gtk::STATE_FLAG_NORMAL);
    set_size_request(8 * 17 - 1 + padding.get_left() + padding.get_right(), 32 * 17 - 1 + padding.get_top() + padding.get_bottom());
    add_events(Gdk::BUTTON_PRESS_MASK);
    dialog.set_transient_for(*window);
    dialog.set_modal(true);
}

bool ShadyGui::PaletteWidget::on_draw(const Cairo::RefPtr<Cairo::Context>& context) {
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    get_style_context()->render_frame(context, 0, 0, width, height);

    for (int j = 0; j < 32; ++j) {
        for (int i = 0; i < 8; ++i) {
            int32_t color = palette->getColor(j * 8 + i);
            context->set_source_rgb(
                ((color >> 16) & 0xff) / 255.0,
                ((color >> 8) & 0xff) / 255.0,
                ((color >> 0) & 0xff) / 255.0);
            context->rectangle(i * 17 + padding.get_left(), j * 17 + padding.get_top(), 16, 16);
            context->fill();
        }
    }
}

static inline Gdk::RGBA to_gdk_rgba(uint32_t color) {
    Gdk::RGBA oColor;
    oColor.set_rgba(
        ((color >> 16) & 0xff) / 255.0,
        ((color >> 8) & 0xff) / 255.0,
        ((color >> 0) & 0xff) / 255.0
        , 1.0);
    return oColor;
}

static inline uint32_t from_gdk_rgba(Gdk::RGBA color) {
    uint32_t oColor = ((int)(color.get_alpha() * 255) & 0xff) << 24;
    oColor += ((int)(color.get_red() * 255) & 0xff) << 16;
    oColor += ((int)(color.get_green() * 255) & 0xff) << 8;
    oColor += ((int)(color.get_blue() * 255) & 0xff);
    return oColor;
}

bool ShadyGui::PaletteWidget::on_button_press_event(GdkEventButton* event) {
	if (curIndex != -1) return false;

    int x = std::floor((event->x - padding.get_left()) / 17.0);
    int y = std::floor((event->y - padding.get_top()) / 17.0);
    if (x >= 0 && x < 8 && y >= 0 && y < 32) {
        curIndex = y * 8 + x;
        printf("index %d\n", curIndex);
        curColor = palette->getColor(curIndex);
        dialog.get_color_selection()->set_current_rgba(to_gdk_rgba(curColor));
        dialog.get_color_selection()->signal_color_changed().connect(
            sigc::mem_fun(*this, &PaletteWidget::onDialogChanged));
        dialog.signal_response().connect(
            sigc::mem_fun(*this, &PaletteWidget::onDialogResponse));
        dialog.show();
    }
}

void ShadyGui::PaletteWidget::onDialogChanged() {
    Gdk::RGBA color = dialog.get_color_selection()->get_current_rgba();
    signal_preview.emit(curIndex, from_gdk_rgba(color));
}

void ShadyGui::PaletteWidget::onDialogResponse(int response) {
	if (curIndex == -1) return;
    else if (response == Gtk::RESPONSE_OK) {
        Gdk::RGBA color = dialog.get_color_selection()->get_current_rgba();
        signal_confirm.emit(curIndex, from_gdk_rgba(color));
    } else {
        signal_preview_cancel.emit(curIndex, curColor);
    }
    dialog.hide();
    curIndex = -1;
}