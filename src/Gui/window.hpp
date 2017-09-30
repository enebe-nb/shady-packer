#pragma once

#include "../Core/package.hpp"

#include <nuklear.h>

namespace ShadyGui {
    class Application;

	class Window {
	protected:
		ShadyGui::Application* const application;
		bool closed = false;
    public:
		inline Window(ShadyGui::Application* application) : application(application) {}
		virtual ~Window() {}
		virtual const char* getName() const = 0;
		inline bool isClosed() const { return closed; }
		virtual void draw(nk_context*, int, int) = 0;
    };

	class MainWindow : public Window {
	protected:
		ShadyCore::Package package;
		const char * lastPath = 0;
		char searchInput[256] = { 0 };
	public:
		inline MainWindow(ShadyGui::Application* application) : Window(application) {}
		inline ShadyCore::Package& getPackage() { return package; }
		inline const char* getName() const override { return "MainWindow"; }
		void draw(nk_context*, int, int) override;
	};
}
