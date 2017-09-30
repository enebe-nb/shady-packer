#pragma once

#include "window.hpp"
#include "dialog.hpp"
#include "editor.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <nuklear.h>
#include <map>

namespace ShadyGui {
    class Application {
    private:
        SDL_Window* window;
        SDL_GLContext glContext;
        nk_context* nkContext;
        nk_font_atlas* nkAtlas;

		struct entryCompare { inline bool operator() (const char* left, const char* right) const { return strcmp(left, right) < 0; } };
		MainWindow mainWindow;
		std::map<const char*, EditorWindow*, entryCompare> editor;
        DialogWindow* dialog = 0;

		bool packageModified = false;
    public:
        Application();
        ~Application();
		int mainLoop();

		inline ShadyCore::Package& getPackage() { return mainWindow.getPackage(); }
		inline bool isDialogOpen() const { return dialog; }
		inline bool isPackageModified() const { return packageModified; }
		inline void setPackageModified(bool m) { packageModified = m; }
		void open(DialogWindow*);
		void open(EditorWindow*);
    };
}
