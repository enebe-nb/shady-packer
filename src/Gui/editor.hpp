#pragma once

#include "application.hpp"
#include "renderer.hpp"

#include <gtkmm/window.h>
#include <gtkmm/entry.h>
#include <gtkmm/menubar.h>
#include <gtkmm/glarea.h>

namespace ShadyGui {
	class EditorWindow : public Gtk::Window {
	public: class EditorComponent;
	protected:
		const ShadyCore::FileType& type;
        ApplicationWindow* parent;
		EditorComponent* component = 0;
	public:
		EditorWindow(ApplicationWindow* parent, ShadyCore::BasePackageEntry& entry);
		virtual ~EditorWindow();
        inline ApplicationWindow* getParent() { return parent; }

        void onCmdSave();
        void onCmdExport();
	};

	class EditorWindow::EditorComponent {
	protected:
        ApplicationWindow* parent;
        EditorWindow* window;
		ShadyCore::Resource* resource = 0;
		Glib::ustring path;
		bool isModified = false;

        virtual const char* getMenuUI() = 0;
        virtual Gtk::Widget* getContent() = 0;
	public:
		friend class EditorWindow;

        inline EditorComponent(ApplicationWindow* parent, EditorWindow* window, ShadyCore::Resource* resource, const char* path)
            : parent(parent), window(window), resource(resource), path(path) {}
		virtual ~EditorComponent();
	};

	class AudioEditor : public EditorWindow::EditorComponent {
	protected:
		AudioRenderer renderer;

		const char* getMenuUI() override;
		Gtk::Widget* getContent() override;
	public:
        AudioEditor(ApplicationWindow*, EditorWindow*, ShadyCore::Resource*, const char*);
        void onStop();
        void onPlay();
        void onLoad();
	};

	class LabelEditor : public EditorWindow::EditorComponent {
	protected:
        Gtk::Entry inputName;
        Gtk::Entry inputOffset;
        Gtk::Entry inputSize;

		const char* getMenuUI() override;
		Gtk::Widget* getContent() override;
	public:
        LabelEditor(ApplicationWindow*, EditorWindow*, ShadyCore::Resource*, const char*);
        void onChanged(Gtk::Entry*);
	};

	class ImageEditor : public EditorWindow::EditorComponent {
	protected:
        SpriteWidget canvas;
        ShadyCore::Palette* palette = 0;
		int curPalette = 0;

		const char* getMenuUI() override;
		Gtk::Widget* getContent() override;
	public:
        ImageEditor(ApplicationWindow*, EditorWindow*, ShadyCore::Resource*, const char*);
        virtual ~ImageEditor();
        void onCmdImage(bool);
        void onCmdPalette(bool);
        bool onKeyPress(GdkEventKey*);
	};

	class PaletteEditor : public EditorWindow::EditorComponent {
	protected:
		ShadyCore::Palette packed;
        SpriteWidget canvasNormal, canvasPacked;
        PaletteWidget canvasPalette;
        ShadyCore::Image* image = 0;
		int curImage = 0, curColorIndex = 1;

		const char* getMenuUI() override;
		Gtk::Widget* getContent() override;
	public:
        PaletteEditor(ApplicationWindow*, EditorWindow*, ShadyCore::Resource*, const char*);
		virtual ~PaletteEditor();
        void onCmdImage(bool);
        bool onKeyPress(GdkEventKey*);
        void onPalettePreview(int, int32_t);
        void onPaletteConfirm(int, int32_t);
	};
}
