#pragma once

#include "window.hpp"
#include "renderer.hpp"

#include <SDL_audio.h>

namespace ShadyGui {
	class EditorWindow : public Window {
	public: class EditorComponent;
	protected:
		const ShadyCore::FileType& type;
		EditorComponent* component;
		char name[15];
		struct nk_rect position;
	public:
		EditorWindow(Application* application, ShadyCore::BasePackageEntry& entry);
		virtual ~EditorWindow();
		inline const char* getName() const override { return name; }
		void draw(nk_context*, int, int) override;

		inline static void closeCallback(EditorWindow* self, bool confirm) { if (confirm) self->closed = true; } // TODO testing
	};

	class EditorWindow::EditorComponent {
	protected:
		Application* application;
		ShadyCore::Resource* resource;
		const char* path;
		bool isModified = false;
	public:
		friend class EditorWindow;

		virtual ~EditorComponent();
		virtual void initialize() = 0;
		virtual void getPreferredSize(int&, int&) const = 0;
		virtual void onGuiLayout(nk_context*) = 0;
	};

	class ImageEditor : public EditorWindow::EditorComponent {
	protected:
		ImageRenderer renderer;
		int curPalette = 0;

		void setPalette(ShadyCore::Palette*);
	public:
		virtual ~ImageEditor();
		void initialize() override;
		void getPreferredSize(int&, int&) const override;
		void onGuiLayout(nk_context*) override;
	};

	class AudioEditor : public EditorWindow::EditorComponent {
	protected:
		AudioRenderer renderer;
	public:
		void initialize() override;
		inline void getPreferredSize(int& width, int& height) const override { width = 240; height = 96; }
		void onGuiLayout(nk_context*) override;
	};

	class LabelEditor : public EditorWindow::EditorComponent {
	protected:
		char inputName[256];
		char inputOffset[256];
		char inputSize[256];
	public:
		void initialize() override;
		inline void getPreferredSize(int& width, int& height) const override { width = 240; height = 144; }
		void onGuiLayout(nk_context*) override;
	};

	/*
	class TextEditor : public EditorWindow::EditorComponent {
	protected:
		bool initialized = false;
		struct nk_text_edit input;
	public:
		virtual ~TextEditor();
		void initialize() override;
		inline void getPreferredSize(int& width, int& height) const override { width = 300; height = 400; }
		void onGuiLayout(nk_context*) override;
	};
	*/

	class PaletteEditor : public EditorWindow::EditorComponent {
	protected:
		ShadyCore::Palette packed;
		ImageRenderer renderNormal, renderPacked;
		int curImage = 0, curColorIndex = 1;
		struct nk_color curColor;

		void setImage(ShadyCore::Image*);
	public:
		virtual ~PaletteEditor();
		void initialize() override;
		inline void getPreferredSize(int& width, int& height) const override { width = 600; height = 400; }
		void onGuiLayout(nk_context*) override;
	};
}