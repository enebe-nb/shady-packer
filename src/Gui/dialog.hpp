#pragma once

#include "../Core/package.hpp"

#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <glibmm/dispatcher.h>
#include <thread>
#include <mutex>

namespace ShadyGui {
	class LongTaskDialog : public Gtk::Dialog {
	private:
        Gtk::Label label;
        Gtk::ProgressBar progress;
        Glib::Dispatcher dispatcher;
		std::thread* thread;
        bool running = false;

		static void taskThread(LongTaskDialog*);
        void on_notify();
	public:
		LongTaskDialog(const Glib::ustring&, bool = false);
	protected:
        std::mutex mutex;
        Glib::ustring curLabel;
        float curProgress;

        virtual void on_show() override;
		virtual void runTask() = 0;
        inline void notify() { dispatcher.emit(); }
	};

	class SavePackageTask : public LongTaskDialog {
	protected:
        ShadyCore::Package& package;
		const Glib::ustring target;
		ShadyCore::Package::Mode mode;

		void runTask() override;
        static void callback(SavePackageTask*, const char*, unsigned int, unsigned int);
	public:
		inline SavePackageTask(ShadyCore::Package& package, const Glib::ustring& target, ShadyCore::Package::Mode mode, bool modal = false)
			: LongTaskDialog("Saving Package", modal), package(package), target(target), mode(mode) {}
	};
}