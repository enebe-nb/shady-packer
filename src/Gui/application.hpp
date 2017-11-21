#pragma once

#include "../Core/package.hpp"

#include <gtkmm/applicationwindow.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <map>

namespace ShadyGui {
    class Application;

    class ApplicationWindow : public Gtk::ApplicationWindow {
    protected:
        ShadyCore::Package package;
        int windowCount = 0;
		bool isModified = false;

        Gtk::SearchEntry searchView;
        Gtk::TreeView entryView;
        Glib::RefPtr<Gtk::ListStore> entryData;
    public:
        friend class Application;
        ApplicationWindow();
        inline ShadyCore::Package& getPackage() { return package; }

        bool on_delete_event(GdkEventAny*) override;
        void onCmdAppend(const Glib::VariantBase&);
        void onCmdSave(const Glib::VariantBase&);
        void onCmdClear();

        sigc::signal<void, bool> signal_package;
        bool onEntryClick(GdkEventButton*);
        void onPackage(bool);
        void onSearch();
    };

    class Application : public Gtk::Application {
    protected:
        static Application* self;
    public:
        inline Application() : Gtk::Application("hisouten.shady.viewer") { self = this; }
        static Application* get() { return self; }

        void on_startup() override;
        void on_activate() override;
        void on_window_added(Gtk::Window*) override;
        void on_window_removed(Gtk::Window*) override;

        void onCmdQuit();
    };
}
