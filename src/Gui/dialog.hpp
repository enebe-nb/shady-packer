#pragma once

#include "window.hpp"

#include <boost/filesystem.hpp>

namespace ShadyGui {
	class DialogWindow : public Window {
	protected:
		virtual void getPreferredSize(int&, int&) const = 0;
		virtual void onGuiLayout(nk_context*) = 0;
	public:
		inline DialogWindow(Application* application) : Window(application) {}
		virtual ~DialogWindow() {}
		void draw(nk_context*, int, int) override;
	};

	class LongTaskDialog : public DialogWindow {
	private:
		const char* const name;
		bool running = false;
	public:
		inline LongTaskDialog(Application* application, const char* name) : DialogWindow(application), name(name) {}
		inline void getPreferredSize(int& width, int& height) const override { width -= 50; height = 100; }
		void onGuiLayout(nk_context*) override;
	protected:
		inline const char* getName() const override { return name; }

		virtual void run() = 0;
		inline static void taskThread(LongTaskDialog* self) { self->run(); self->closed = true; }
	};

	class SavePackageTask : public LongTaskDialog {
	protected:
		boost::filesystem::path target;
		ShadyCore::Package::Mode mode;

		void run() override;
	public:
		inline SavePackageTask(Application* application, boost::filesystem::path target, ShadyCore::Package::Mode mode)
			: LongTaskDialog(application, "Saving Package"), target(target), mode(mode) {}
	};

	class ConfirmDialog : public DialogWindow {
	public: typedef void(*Callback)(void*, bool);
	protected:
		const char* const title;
		const char* const message;
		void* const userdata;
		Callback callback;
		inline const char* getName() const override { return title; }
	public:
		inline ConfirmDialog(Application*, void* userdata, Callback callback, const char* title = 0, const char* message = 0)
			: DialogWindow(application), userdata(userdata), callback(callback), title(title ? title : "Confirm Dialog"), message(message ? message : "Are you sure?") {}
		inline void getPreferredSize(int& width, int& height) const override { width = 300; height = 124; }
		void onGuiLayout(nk_context*) override;
	};
}