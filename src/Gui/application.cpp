#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"

#include <gtkmm/scrolledwindow.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/builder.h>
#include <glibmm/convert.h>

ShadyGui::Application* ShadyGui::Application::self = 0;

namespace {
    class EntryModel : public Gtk::TreeModel::ColumnRecord {
        public:
            Gtk::TreeModelColumn<Glib::ustring> storage;
            Gtk::TreeModelColumn<Glib::ustring> name;
            inline EntryModel() { add(storage); add(name); }
    };
    static EntryModel entryModel;
}

ShadyGui::ApplicationWindow::ApplicationWindow() : Gtk::ApplicationWindow() {
    set_title("Shady Viewer");
    set_default_size(300, 500);

	auto action = Gio::SimpleAction::create("append", Glib::Variant<Glib::ustring>::variant_type());
	action->signal_activate().connect(sigc::mem_fun(*this, &ApplicationWindow::onCmdAppend));
	add_action(action);
	action = Gio::SimpleAction::create("save", Glib::Variant<Glib::ustring>::variant_type());
	action->signal_activate().connect(sigc::mem_fun(*this, &ApplicationWindow::onCmdSave));
	add_action(action);
    add_action("clear", sigc::mem_fun(*this, &ApplicationWindow::onCmdClear));
    add_action("quit", sigc::mem_fun(*this, &ApplicationWindow::close));
    signal_package.connect(sigc::mem_fun(*this, &ApplicationWindow::onPackage));
    searchView.signal_search_changed().connect(sigc::mem_fun(*this, &ApplicationWindow::onSearch));

    Gtk::Box* container = Gtk::manage(new Gtk::Box());
    container->set_orientation(Gtk::ORIENTATION_VERTICAL);
    add(*container);

    container->pack_start(searchView, Gtk::PACK_SHRINK);
    Gtk::ScrolledWindow* scroll = Gtk::manage(new Gtk::ScrolledWindow());
    scroll->add(entryView); scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    container->pack_start(*scroll, Gtk::PACK_EXPAND_WIDGET);

    entryData = Gtk::ListStore::create(entryModel);
    entryView.set_model(entryData);
    entryView.append_column("Storage", entryModel.storage);
    entryView.append_column("Name", entryModel.name);
    entryView.signal_button_press_event().connect(sigc::mem_fun(*this, &ApplicationWindow::onEntryClick));
    signal_package.emit(false);

    show_all_children();
}

bool ShadyGui::ApplicationWindow::on_delete_event(GdkEventAny* event) {
    if (isModified) {
        Gtk::MessageDialog dialog(*this, "Close confirmation", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
        dialog.set_secondary_text("You have unsaved changes, do you want to quit and discard changes?");
        if (dialog.run() != Gtk::RESPONSE_YES) return true;
    }
    return Gtk::ApplicationWindow::on_delete_event(event);
}

void ShadyGui::ApplicationWindow::onCmdAppend(const Glib::VariantBase& param) {
    bool isFolder = param.equal(Glib::Variant<Glib::ustring>::create("folder"));
    Gtk::FileChooserDialog dialog("Select the package to load",
        isFolder ? Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER : Gtk::FILE_CHOOSER_ACTION_OPEN);

    dialog.set_transient_for(*this);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Append", Gtk::RESPONSE_OK);
    dialog.set_select_multiple(true);

    if (!isFolder) {
        auto filter = Gtk::FileFilter::create();
        filter->set_name("Package Files");
        filter->add_pattern("*.dat");
        filter->add_pattern("*.zip");
        dialog.add_filter(filter);
    }

    int result = dialog.run();
    dialog.hide();
    if (result == Gtk::RESPONSE_OK) {
        auto filenames = dialog.get_filenames();
        for (auto f : filenames) {
            package.appendPackage(f.c_str());
        }
        signal_package.emit(false);
    }
}

void ShadyGui::ApplicationWindow::onCmdSave(const Glib::VariantBase& param) {
    auto paramValue = param.cast_dynamic<Glib::Variant<Glib::ustring> >(param).get();
    bool isFolder = paramValue == "dir";
    Gtk::FileChooserDialog dialog("Select the package to load",
        isFolder ? Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER : Gtk::FILE_CHOOSER_ACTION_SAVE);

    dialog.set_transient_for(*this);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Save", Gtk::RESPONSE_OK);
    dialog.set_do_overwrite_confirmation(true);

    if (!isFolder) {
        auto filter = Gtk::FileFilter::create();
        filter->set_name("Package Files");
        filter->add_pattern("*.dat");
        filter->add_pattern("*.zip");
        dialog.add_filter(filter);
    }

    int result = dialog.run();
    dialog.hide();
    if (result == Gtk::RESPONSE_OK) {
        auto filename = dialog.get_filename();
        ShadyGui::SavePackageTask dialog(package, filename,
            paramValue == "data" ? ShadyCore::Package::DATA_MODE :
            paramValue == "zip" ? ShadyCore::Package::ZIP_MODE :
            ShadyCore::Package::DIR_MODE);

        dialog.run();
        signal_package.emit(isModified = false);
    }
}

void ShadyGui::ApplicationWindow::onCmdClear() {
    if (isModified) {
        Gtk::MessageDialog dialog(*this, "Clear confirmation", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
        dialog.set_secondary_text("You have unsaved changes, do you want to discard any changes?");
        if (dialog.run() != Gtk::RESPONSE_YES) return;
    }
    package.clear();
    signal_package.emit(isModified = false);
}

bool ShadyGui::ApplicationWindow::onEntryClick(GdkEventButton* event) {
    if (event->type != GDK_2BUTTON_PRESS || event->button != 1) return false;

    auto selection = entryView.get_selection();
    Glib::ustring name = selection->get_selected()->get_value(entryModel.name);
    EditorWindow* window = new EditorWindow(this, *package.findFile(name.c_str()));
    Application::get()->add_window(*window);
    window->show_all();
}

void ShadyGui::ApplicationWindow::onPackage(bool modified) {
    isModified = isModified || modified;

    entryData->clear();
    { std::lock_guard<ShadyCore::Package> lock(package);
    for (auto& entry : package) {
        if (Glib::ustring(entry.getName()).find(searchView.get_text()) == -1) continue;
        auto row = *entryData->append();
        row[entryModel.storage] =
            entry.getType() == ShadyCore::BasePackageEntry::TYPE_FILE ? "Filesystem"
            : entry.getType() == ShadyCore::BasePackageEntry::TYPE_DATA ? "Data package"
            : entry.getType() == ShadyCore::BasePackageEntry::TYPE_ZIP ? "Zip package"
            : entry.getType() == ShadyCore::BasePackageEntry::TYPE_STREAM ? "Memory"
            : "";
        row[entryModel.name] = Glib::filename_display_name(entry.getName());
    } }
}

void ShadyGui::ApplicationWindow::onSearch() { signal_package.emit(false); }

static const char appMenuUI[] =
"<interface>"
    "<menu id='menu'>"
        "<submenu>"
            "<attribute name='label'>_File</attribute>"
            "<item>"
                "<attribute name='label'>_Append Package</attribute>"
                "<attribute name='action'>win.append</attribute>"
                "<attribute name='target'>file</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>Append Folder</attribute>"
                "<attribute name='action'>win.append</attribute>"
                "<attribute name='target'>folder</attribute>"
            "</item>"
            "<submenu>"
                "<attribute name='label'>_Save</attribute>"
                "<item>"
                    "<attribute name='label'>as _Data package</attribute>"
                    "<attribute name='action'>win.save</attribute>"
                    "<attribute name='target'>data</attribute>"
                "</item>"
                "<item>"
                    "<attribute name='label'>as _Zip package</attribute>"
                    "<attribute name='action'>win.save</attribute>"
                    "<attribute name='target'>zip</attribute>"
                "</item>"
                "<item>"
                    "<attribute name='label'>into _Folder</attribute>"
                    "<attribute name='action'>win.save</attribute>"
                    "<attribute name='target'>dir</attribute>"
                "</item>"
            "</submenu>"
            "<item>"
                "<attribute name='label'>_Clear Package</attribute>"
                "<attribute name='action'>win.clear</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>_Quit</attribute>"
                "<attribute name='action'>win.quit</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>_Quit All</attribute>"
                "<attribute name='action'>app.quit</attribute>"
            "</item>"
        "</submenu>"
    "</menu>"
"</interface>";

void ShadyGui::Application::on_startup() {
    Gtk::Application::on_startup();

    auto builder = Gtk::Builder::create_from_string(appMenuUI);
    set_menubar(Glib::RefPtr<Gio::Menu>::cast_dynamic(builder->get_object("menu")));

    add_action("quit", sigc::mem_fun(*this, &Application::onCmdQuit));
}

void ShadyGui::Application::on_activate() {
    Gtk::Application::on_activate();

    ApplicationWindow* window = new ApplicationWindow();
    add_window(*window);
    window->show_all();
}

void ShadyGui::Application::on_window_added(Gtk::Window* window) {
    Gtk::Application::on_window_added(window);
    if (EditorWindow* editor = dynamic_cast<EditorWindow*>(window)) {
        ++editor->getParent()->windowCount;
    } else if (ApplicationWindow* parent = dynamic_cast<ApplicationWindow*>(window)) {
        ++parent->windowCount;
    }
}

void ShadyGui::Application::on_window_removed(Gtk::Window* window) {
    Gtk::Application::on_window_removed(window);
    if (EditorWindow* editor = dynamic_cast<EditorWindow*>(window)) {
        ApplicationWindow* parent = editor->getParent();
        delete editor;
        if (!--parent->windowCount) delete parent;
    } else if (ApplicationWindow* parent = dynamic_cast<ApplicationWindow*>(window)) {
        if (!--parent->windowCount) delete parent;
    }
}

void ShadyGui::Application::onCmdQuit() {
    for (auto window : get_windows()){
        window->close();
    }
}

/*
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
*/

int main(int argc, char* argv[]) {
//#ifdef _WIN32
	//SetUnhandledExceptionFilter(ExceptionHandler);
//#endif
    ShadyGui::Application application;
    application.run(argc, argv);
}
