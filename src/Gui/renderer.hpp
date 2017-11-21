#pragma once

#include "../Core/resource.hpp"

#include <glibmm/dispatcher.h>
#include <gtkmm/glarea.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/colorselection.h>
#include <RtAudio.h>

namespace ShadyGui {
	class AudioRenderer {
	protected:
        Glib::Dispatcher dispatcher;
        RtAudio::StreamParameters params;
        RtAudio::StreamOptions audioOptions;
        RtAudioFormat audioFormat;
        ShadyCore::Sfx* audio = 0;
        uint16_t frameSize = 0;
        uint32_t current = 0;
	public:
		AudioRenderer();
		virtual ~AudioRenderer();
		void setAudio(ShadyCore::Sfx*);
		bool playAudio();
        void stopAudio();

        static int streamCallback(void*, void*, unsigned int, double, RtAudioStreamStatus, void*);
	};

    class SpriteWidget : public Gtk::DrawingArea {
    protected:
        // TODO rotation
		ShadyCore::Image* image = 0;
		ShadyCore::Palette* palette = 0;
		int tx = 0, ty = 0;
		double mx, my, scale = 1;
		Cairo::RefPtr<Cairo::SurfacePattern> surface;
		uint8_t* surfaceData = 0;

		bool on_draw(const Cairo::RefPtr<Cairo::Context>&) override;
        bool on_button_press_event(GdkEventButton*) override;
        bool on_motion_notify_event(GdkEventMotion*) override;
        bool on_scroll_event(GdkEventScroll*) override;
		
		void buildImageData();
    public:
        SpriteWidget();
        virtual ~SpriteWidget();

		void setImage(ShadyCore::Image*);
		void setPalette(ShadyCore::Palette*);
    };

    class PaletteWidget : public Gtk::DrawingArea {
    protected:
        ShadyCore::Palette* palette;
		Gtk::ColorSelectionDialog dialog;
        Gtk::Border padding;
		int curIndex = -1, curColor;

        bool on_draw(const Cairo::RefPtr<Cairo::Context>&) override;
        bool on_button_press_event(GdkEventButton*) override;
		Glib::Dispatcher changed;
    public:
        PaletteWidget(Gtk::Window*, ShadyCore::Palette*);
        void onDialogChanged();
        void onDialogResponse(int);
		sigc::signal<void, int, int32_t> signal_preview;
		sigc::signal<void, int, int32_t> signal_preview_cancel;
		sigc::signal<void, int, int32_t> signal_confirm;
    };
}
