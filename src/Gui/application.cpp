#include "application.hpp"
#include "window.hpp"
#include "../Core/package.hpp"

#include <demo/sdl_opengl2/nuklear_sdl_gl2.h>
#include <boost/locale.hpp>
#include <boost/thread.hpp>
#include <tinyfiledialogs.h>
#include <boost/filesystem.hpp>

const char* APPLICATION_TITTLE = "ShadyPacker";

ShadyGui::Application::Application() : mainWindow(this) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    window = SDL_CreateWindow(APPLICATION_TITTLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
        SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);

    glContext = SDL_GL_CreateContext(window);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    nkContext = nk_sdl_init(window);
    nk_sdl_font_stash_begin(&nkAtlas);
    nk_sdl_font_stash_end();

    nkContext->style.checkbox.cursor_hover.data.color = nk_color{0xd0, 0xd0, 0xd0, 0xff};
    nkContext->style.checkbox.cursor_normal.data.color = nk_color{0xd0, 0xd0, 0xd0, 0xff};
}

ShadyGui::Application::~Application() {
    nk_sdl_shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static void closeCallback(int* exitValue, bool confirm) {
	if (confirm) *exitValue = 0;
}

int ShadyGui::Application::mainLoop() {
    if (!window) return 1;
    SDL_ShowWindow(window);

    int exitValue = -1;
    while(exitValue < 0) {
        nk_input_begin(nkContext);
		for (SDL_Event event; SDL_PollEvent(&event);) {
			if (event.type == SDL_QUIT && !dialog) {
				if (isPackageModified()) {
					open(new ConfirmDialog(this, &exitValue, (ConfirmDialog::Callback)&closeCallback
						, "Unsaved Package modifications", "The package was modified, do you want discard any modification?"));
				} else exitValue = 0;
			//} else if (dialog) { // Do Nothing
			} else if (event.type == SDL_DROPBEGIN) {
				//printf("DROPBEGIN\n");
			} else if (event.type == SDL_DROPTEXT) {
				//printf("DROPTEXT %s\n", event.drop.file);
			} else if (event.type == SDL_DROPFILE) {
				//printf("DROPFILE %s\n", event.drop.file);
				//dropFiles.append(event.drop.file);
			} else if (event.type == SDL_DROPCOMPLETE) {
				//printf("DROPCOMPLETE\n");
				//dropFiles.setDone();
			} else nk_sdl_handle_event(&event);
        }
        nk_input_end(nkContext);

        int width, height;
        SDL_GL_GetDrawableSize(window, &width, &height);
		mainWindow.draw(nkContext, width, height);

		auto iter = editor.begin(); while (iter != editor.end()) {
			if (iter->second->isClosed()) {
				delete iter->second;
				iter = editor.erase(iter);
			} else {
				iter->second->draw(nkContext, width, height);
				++iter;
			}
		}

		if (dialog) 
			if (dialog->isClosed()) {
				delete dialog;
				dialog = 0;
			} else dialog->draw(nkContext, width, height);

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        nk_sdl_render(NK_ANTI_ALIASING_ON);
        SDL_GL_SwapWindow(window);
    }

    SDL_HideWindow(window);
    return 0;
}

void ShadyGui::Application::open(DialogWindow* dialog) {
	if (this->dialog) delete dialog;
	else this->dialog = dialog;
}

void ShadyGui::Application::open(EditorWindow* editor) {
	if (this->editor.find(editor->getName()) == this->editor.end()) {
		this->editor[editor->getName()] = editor;
	} else delete editor;
}

#ifdef _WIN32
#include <Dbghelp.h>
#pragma comment ( lib, "dbghelp.lib" )

LONG CALLBACK ExceptionHandler(EXCEPTION_POINTERS* e) {
	char name[MAX_PATH]; {
		char* nameEnd = name + GetModuleFileNameA(GetModuleHandleA(0), name, MAX_PATH);
		SYSTEMTIME t; GetSystemTime(&t);
		wsprintfA(nameEnd - strlen(".exe"), "_%4d%02d%02d_%02d%02d%02d.dmp", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	}

	HANDLE hFile = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
	exceptionInfo.ThreadId = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = e;
	exceptionInfo.ClientPointers = FALSE;

	auto dumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory), e ? &exceptionInfo : nullptr, nullptr, nullptr);
	CloseHandle(hFile);

	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
	SetUnhandledExceptionFilter(ExceptionHandler);
#endif
	ShadyGui::Application app;
	return app.mainLoop();
}
