#include "editor.hpp"

#include <giomm/simpleactiongroup.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stock.h>
#include <gtkmm/builder.h>
#include <gtkmm/menu.h>
#include <fstream>
#include <sstream>
#include <epoxy/gl.h>

static const char editorMenuUI[] =
"<interface>"
    "<menu id='menu'>"
        "<submenu>"
            "<attribute name='label'>_File</attribute>"
            "<item>"
                "<attribute name='label'>_Save</attribute>"
                "<attribute name='action'>editor.save</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>E_xport</attribute>"
                "<attribute name='action'>editor.export</attribute>"
            "</item>"
        "</submenu>"
    "</menu>"
"</interface>";

ShadyGui::EditorWindow::EditorWindow(ApplicationWindow* parent, ShadyCore::BasePackageEntry& entry)
	: Gtk::Window(), parent(parent), type(ShadyCore::FileType::get(entry)) {
    set_title(entry.getName());

    ShadyCore::Resource* resource = ShadyCore::readResource(type, entry.open()); entry.close();
    if (!resource) {
        add(*Gtk::manage(new Gtk::Label("This resource can't be read.")));
        return;
    }

	switch (type.type) {
		case ShadyCore::FileType::TYPE_IMAGE: component = new ImageEditor(parent, this, resource, entry.getName()); break;
		case ShadyCore::FileType::TYPE_LABEL: component = new LabelEditor(parent, this, resource, entry.getName()); break;
		case ShadyCore::FileType::TYPE_SFX: component = new AudioEditor(parent, this, resource, entry.getName()); break;
		//case ShadyCore::FileType::TYPE_TEXT: component = new TextEditor; break;
		case ShadyCore::FileType::TYPE_PALETTE: component = new PaletteEditor(parent, this, resource, entry.getName()); break;
		default:
            delete resource;
            add(*Gtk::manage(new Gtk::Label("This editor wasn't implemented.")));
            return;
	}

    Gtk::Box* container = Gtk::manage(new Gtk::Box());
    container->set_orientation(Gtk::ORIENTATION_VERTICAL);
    add(*container);

    auto actionGroup = Gio::SimpleActionGroup::create();
    actionGroup->add_action("save", sigc::mem_fun(*this, &EditorWindow::onCmdSave));
    actionGroup->add_action("export", sigc::mem_fun(*this, &EditorWindow::onCmdExport));
    insert_action_group("editor", actionGroup);

    auto builder = Gtk::Builder::create_from_string(component->getMenuUI());
    Gtk::MenuBar* menubar = Gtk::manage(new Gtk::MenuBar(
        Glib::RefPtr<Gio::MenuModel>::cast_dynamic(builder->get_object("menu"))));
    container->pack_start(*menubar, Gtk::PACK_SHRINK);

    Gtk::ScrolledWindow* scroll = Gtk::manage(new Gtk::ScrolledWindow());
    scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scroll->add(*component->getContent());
    container->pack_start(*scroll, Gtk::PACK_EXPAND_WIDGET);
}

void ShadyGui::EditorWindow::onCmdSave() {
    std::stringstream datastream;
    ShadyCore::writeResource(component->resource, datastream, type.isEncrypted);
    parent->getPackage().appendFile(component->path.c_str(), datastream);
    parent->signal_package.emit(true);
    component->isModified = false;
}

void ShadyGui::EditorWindow::onCmdExport() {
    Gtk::FileChooserDialog dialog("Select where to save", Gtk::FILE_CHOOSER_ACTION_SAVE);

    dialog.set_transient_for(*this);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Save", Gtk::RESPONSE_OK);
    dialog.set_do_overwrite_confirmation(true);

    auto filter = Gtk::FileFilter::create();
    filter->set_name(type.normalExt);
    filter->add_pattern(Glib::ustring("*") + type.normalExt);
    dialog.add_filter(filter);
    filter = Gtk::FileFilter::create();
    filter->set_name(type.inverseExt);
    filter->add_pattern(Glib::ustring("*") + type.inverseExt);
    dialog.add_filter(filter);

    int result = dialog.run();
    dialog.hide();
    if (result == Gtk::RESPONSE_OK) {
        std::ofstream output(dialog.get_filename().c_str(), std::ios::binary);
        ShadyCore::writeResource(component->resource, output,
            dialog.get_filter()->get_name() == type.normalExt ? type.isEncrypted : !type.isEncrypted);
    }
}

ShadyGui::EditorWindow::~EditorWindow() { if(component) delete component; }
ShadyGui::EditorWindow::EditorComponent::~EditorComponent() { if(resource) delete resource; }

ShadyGui::LabelEditor::LabelEditor(ApplicationWindow* parent, EditorWindow* window, ShadyCore::Resource* resource, const char* path)
    : EditorWindow::EditorComponent(parent, window, resource, path) {
    window->set_default_size(300, 200);
    ShadyCore::LabelResource* label = (ShadyCore::LabelResource*)resource;
    inputName.set_text(label->getName());
    inputOffset.set_text(Glib::ustring::format(label->getOffset() / 44100.f));
    inputSize.set_text(Glib::ustring::format(label->getSize() / 44100.f));

    inputName.signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &LabelEditor::onChanged), &inputName));
    inputOffset.signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &LabelEditor::onChanged), &inputOffset));
    inputSize.signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &LabelEditor::onChanged), &inputSize));
}

const char* ShadyGui::LabelEditor::getMenuUI() { return editorMenuUI; }
Gtk::Widget* ShadyGui::LabelEditor::getContent() {
    Gtk::Grid* container = Gtk::manage(new Gtk::Grid);
    container->set_size_request(200, 70);
    container->set_column_homogeneous(true);
    Gtk::Label* label = Gtk::manage(new Gtk::Label("Name"));
    container->attach(*label, 0, 0, 1, 1);
    container->attach(inputName, 1, 0, 2, 1);
    label = Gtk::manage(new Gtk::Label("Offset"));
    container->attach(*label, 0, 1, 1, 1);
    container->attach(inputOffset, 1, 1, 2, 1);
    label = Gtk::manage(new Gtk::Label("Size"));
    container->attach(*label, 0, 2, 1, 1);
    container->attach(inputSize, 1, 2, 2, 1);
    return container;
}

void ShadyGui::LabelEditor::onChanged(Gtk::Entry* input) {
    ShadyCore::LabelResource* label = (ShadyCore::LabelResource*)resource;
    if (input == &inputName) {
        uint32_t offset = label->getOffset();
        uint32_t size = label->getSize();
        label->initialize(inputName.get_text().c_str());
        label->setOffset(offset);
        label->setSize(size);
        isModified = true;
    } else {
        double offset, size;
        std::stringstream offsetStr(inputOffset.get_text());
        std::stringstream sizeStr(inputSize.get_text());
        if ((bool)(offsetStr >> offset) && (bool)(sizeStr >> size)) {
            label->setOffset(offset * 44100);
            label->setSize(size * 44100);
            isModified = true;
            printf("Modified\n");
        }
    }
}

ShadyGui::AudioEditor::AudioEditor(ApplicationWindow* parent, EditorWindow* window, ShadyCore::Resource* resource, const char* path)
    : EditorWindow::EditorComponent(parent, window, resource, path) {
    window->set_default_size(200, 80);
    renderer.setAudio((ShadyCore::Sfx*)resource);
}

const char* ShadyGui::AudioEditor::getMenuUI() { return editorMenuUI; }
Gtk::Widget* ShadyGui::AudioEditor::getContent() {
    Gtk::Box* container = Gtk::manage(new Gtk::Box);
    container->set_size_request(200, 20);
    container->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    Gtk::Button* button = Gtk::manage(new Gtk::Button(Gtk::Stock::MEDIA_STOP));
    button->signal_clicked().connect(sigc::mem_fun(*this, &AudioEditor::onStop));
    container->pack_start(*button, Gtk::PACK_EXPAND_WIDGET);
    button = Gtk::manage(new Gtk::Button(Gtk::Stock::MEDIA_PLAY));
    button->signal_clicked().connect(sigc::mem_fun(*this, &AudioEditor::onPlay));
    container->pack_start(*button, Gtk::PACK_EXPAND_WIDGET);
    button = Gtk::manage(new Gtk::Button(Gtk::Stock::OPEN));
    button->signal_clicked().connect(sigc::mem_fun(*this, &AudioEditor::onLoad));
    container->pack_start(*button, Gtk::PACK_EXPAND_WIDGET);
    return container;
}

void ShadyGui::AudioEditor::onStop() { renderer.stopAudio(); }
void ShadyGui::AudioEditor::onPlay() { renderer.playAudio(); }
void ShadyGui::AudioEditor::onLoad() {
    Gtk::FileChooserDialog dialog(*window, "Select a sound file to load", Gtk::FILE_CHOOSER_ACTION_OPEN);

    //dialog.set_transient_for(*parent);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Open", Gtk::RESPONSE_OK);

    auto filter = Gtk::FileFilter::create();
    filter->set_name("Sound Files");
    filter->add_pattern("*.wav");
    filter->add_pattern("*.cv3");
    dialog.add_filter(filter);

    int result = dialog.run();
    dialog.hide();
    if (result == Gtk::RESPONSE_OK) {
        renderer.stopAudio();
        std::ifstream input(dialog.get_filename().c_str(), std::ios::binary);
        const ShadyCore::FileType& type = ShadyCore::FileType::get(dialog.get_filename().c_str(), input);
        ShadyCore::Resource* newResource = ShadyCore::readResource(type, input);
        if (newResource) {
            delete resource;
            resource = newResource;
            isModified = true;
        } else {
            Gtk::MessageDialog dialog(*window, "Load failed", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
            dialog.set_secondary_text("Sound file load failed.");
            dialog.run();
        }
    }
}

static ShadyCore::Image* loadImage(ShadyCore::Package& package, const char* path, int& index) {
	const char* e = strrchr(path, '/');
	if (!e) e = strrchr(path, '_');
	if (!e) e = path - 1;

	int i = 0;
	const char *firstFound = 0, *lastFound = 0;
	for (auto& entry : package) {
		if (strncmp(path, entry.getName(), e - path + 1) == 0) {
			const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);
			if (type != ShadyCore::FileType::TYPE_IMAGE) continue;

			if (!firstFound) firstFound = entry.getName();
			lastFound = entry.getName();

			if (i++ == index) {
				ShadyCore::Resource* resource = ShadyCore::readResource(type, entry.open()); entry.close();
				return (ShadyCore::Image*)resource;
			}
		}
	}

	if (!firstFound) return 0;
	const char* target;
	if (index == -1) {
		index = i - 1;
		target = lastFound;
	} else {
		index = 0;
		target = firstFound;
	}

	ShadyCore::BasePackageEntry& entry = *package.findFile(target);
	const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);
	if (type != ShadyCore::FileType::TYPE_IMAGE) throw; // TODO better handling
	ShadyCore::Resource* resource = ShadyCore::readResource(type, entry.open()); entry.close();
	return (ShadyCore::Image*)resource;
}

static ShadyCore::Palette* loadPalette(ShadyCore::Package& package, const char* path, int index) {
	char buffer[256];

	const char* e = strrchr(path, '/');
	if (!e) e = strrchr(path, '_');
	if (!e) e = path - 1;

	ShadyCore::Package::iterator iter;
	memcpy(buffer, path, e - path + 1);
	sprintf(buffer + (e - path) + 1, "palette%03d.act", index);
	if ((iter = package.findFile(buffer)) != package.end()) {
		ShadyCore::Palette* palette = new ShadyCore::Palette;
		ShadyCore::readResource(palette, iter->open(), false);
		iter->close();
		return palette;
	}

	sprintf(buffer + (e - path) + 1, "palette%03d.pal", index);
	if ((iter = package.findFile(buffer)) != package.end()) {
		ShadyCore::Palette* palette = new ShadyCore::Palette;
		ShadyCore::readResource(palette, iter->open(), true);
		iter->close();
		return palette;
	}

	return 0;
}

static const char imageEditorMenuUI[] =
"<interface>"
    "<menu id='menu'>"
        "<submenu>"
            "<attribute name='label'>_View</attribute>"
            "<item>"
                "<attribute name='label'>_Previous Image</attribute>"
                "<attribute name='action'>image.previous</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>_Next Image</attribute>"
                "<attribute name='action'>image.next</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>Previous Palette</attribute>"
                "<attribute name='action'>image.previouspalette</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>Next Palette</attribute>"
                "<attribute name='action'>image.nextpalette</attribute>"
            "</item>"
        "</submenu>"
    "</menu>"
"</interface>";

ShadyGui::ImageEditor::ImageEditor(ApplicationWindow* parent, EditorWindow* window, ShadyCore::Resource* resource, const char* path)
    : EditorWindow::EditorComponent(parent, window, resource, path) {
	canvas.setImage((ShadyCore::Image*)resource);
    palette = loadPalette(parent->getPackage(), path, curPalette);
	canvas.setPalette(palette);

    window->set_default_size(400, 400);
    window->add_events(Gdk::KEY_PRESS_MASK);
    window->signal_key_press_event().connect(sigc::mem_fun(*this, &ImageEditor::onKeyPress), false);
    auto actionGroup = Gio::SimpleActionGroup::create();
    actionGroup->add_action("previous", sigc::bind(sigc::mem_fun(*this, &ImageEditor::onCmdImage), false));
    actionGroup->add_action("next", sigc::bind(sigc::mem_fun(*this, &ImageEditor::onCmdImage), true));
    actionGroup->add_action("previouspalette", sigc::bind(sigc::mem_fun(*this, &ImageEditor::onCmdPalette), false));
    actionGroup->add_action("nextpalette", sigc::bind(sigc::mem_fun(*this, &ImageEditor::onCmdPalette), true));
    window->insert_action_group("image", actionGroup);
}

ShadyGui::ImageEditor::~ImageEditor() { if (palette) delete palette; }
const char* ShadyGui::ImageEditor::getMenuUI() { return imageEditorMenuUI; }
Gtk::Widget* ShadyGui::ImageEditor::getContent() { return &canvas; }

void ShadyGui::ImageEditor::onCmdImage(bool isNext) {
    if (isModified) {
        Gtk::MessageDialog dialog(*window, "Disabled operation", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
        dialog.set_secondary_text("You must save the image before this operation.");
        dialog.run();
        return;
    }

    ShadyCore::Package& package = parent->getPackage();
    for (auto iter = package.findFile(path.c_str()); isNext ? ++iter != package.end() : iter-- != package.begin();) {
        const ShadyCore::FileType& iterType = ShadyCore::FileType::get(*iter);
        if (iterType == ShadyCore::FileType::TYPE_IMAGE) {
            ShadyCore::readResource(resource, iter->open(), iterType.isEncrypted); iter->close();
            canvas.setImage((ShadyCore::Image*)resource);
            window->set_title(path = iter->getName());
            return;
        }
    }
}

void ShadyGui::ImageEditor::onCmdPalette(bool isNext) {
    if (palette) delete palette;

    if (isNext) palette = loadPalette(parent->getPackage(), path.c_str(), ++curPalette > 7 ? (curPalette = 0) : curPalette);
    else palette = loadPalette(parent->getPackage(), path.c_str(), --curPalette < 0 ? (curPalette = 7) : curPalette);

    canvas.setPalette(palette);
}

bool ShadyGui::ImageEditor::onKeyPress(GdkEventKey* event) {
    switch(event->keyval) {
    case GDK_KEY_Left: case GDK_KEY_KP_Left:
        window->get_action_group("image")->activate_action("previous"); break;
    case GDK_KEY_Right: case GDK_KEY_KP_Right:
        window->get_action_group("image")->activate_action("next"); break;
    case GDK_KEY_Up: case GDK_KEY_KP_Up:
        window->get_action_group("image")->activate_action("previouspalette"); break;
    case GDK_KEY_Down: case GDK_KEY_KP_Down:
        window->get_action_group("image")->activate_action("nextpalette"); break;
    default: return false;
    } return true;
}

static const char paletteEditorMenuUI[] =
"<interface>"
    "<menu id='menu'>"
        "<submenu>"
            "<attribute name='label'>_File</attribute>"
            "<item>"
                "<attribute name='label'>_Save</attribute>"
                "<attribute name='action'>editor.save</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>E_xport</attribute>"
                "<attribute name='action'>editor.export</attribute>"
            "</item>"
        "</submenu>"
        "<submenu>"
            "<attribute name='label'>_View</attribute>"
            "<item>"
                "<attribute name='label'>_Previous Image</attribute>"
                "<attribute name='action'>image.previous</attribute>"
            "</item>"
            "<item>"
                "<attribute name='label'>_Next Image</attribute>"
                "<attribute name='action'>image.next</attribute>"
            "</item>"
        "</submenu>"
    "</menu>"
"</interface>";

ShadyGui::PaletteEditor::PaletteEditor(ApplicationWindow* parent, EditorWindow* window, ShadyCore::Resource* resource, const char* path)
    : EditorWindow::EditorComponent(parent, window, resource, path), canvasPalette(window, (ShadyCore::Palette*)resource) {
	canvasNormal.setPalette((ShadyCore::Palette*)resource);

	for (int i = 0; i < 256; ++i) {
		packed.setColor(i, ShadyCore::Palette::unpackColor(ShadyCore::Palette::packColor(((ShadyCore::Palette*)resource)->getColor(i))));
	} canvasPacked.setPalette(&packed);

	image = loadImage(parent->getPackage(), path, curImage);
    canvasNormal.setImage(image);
    canvasPacked.setImage(image);

    canvasPalette.signal_preview.connect(sigc::mem_fun(*this, &PaletteEditor::onPalettePreview));
    canvasPalette.signal_preview_cancel.connect(sigc::mem_fun(*this, &PaletteEditor::onPalettePreview));
    canvasPalette.signal_confirm.connect(sigc::mem_fun(*this, &PaletteEditor::onPaletteConfirm));

    window->set_default_size(800, 600);
    window->add_events(Gdk::KEY_PRESS_MASK);
    window->signal_key_press_event().connect(sigc::mem_fun(*this, &PaletteEditor::onKeyPress), false);
    auto actionGroup = Gio::SimpleActionGroup::create();
    actionGroup->add_action("previous", sigc::bind(sigc::mem_fun(*this, &PaletteEditor::onCmdImage), false));
    actionGroup->add_action("next", sigc::bind(sigc::mem_fun(*this, &PaletteEditor::onCmdImage), true));
    window->insert_action_group("image", actionGroup);
}

ShadyGui::PaletteEditor::~PaletteEditor() { if (image) delete image; }
const char* ShadyGui::PaletteEditor::getMenuUI() { return paletteEditorMenuUI; }

Gtk::Widget* ShadyGui::PaletteEditor::getContent() {
    Gtk::Box* container = Gtk::manage(new Gtk::Box());
    container->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    container->pack_start(canvasNormal, Gtk::PACK_EXPAND_WIDGET);
    container->pack_start(canvasPacked, Gtk::PACK_EXPAND_WIDGET);
    container->pack_start(canvasPalette, Gtk::PACK_SHRINK);
    return container;
}

void ShadyGui::PaletteEditor::onCmdImage(bool isNext) {
    if (image) delete image;
    image = loadImage(parent->getPackage(), path.c_str(), isNext ? ++curImage : --curImage);

    canvasNormal.setImage(image);
    canvasPacked.setImage(image);
}

bool ShadyGui::PaletteEditor::onKeyPress(GdkEventKey* event) {
    switch(event->keyval) {
    case GDK_KEY_Left: case GDK_KEY_KP_Left:
        window->get_action_group("image")->activate_action("previous"); break;
    case GDK_KEY_Right: case GDK_KEY_KP_Right:
        window->get_action_group("image")->activate_action("next"); break;
    default: return false;
    } return true;
}

void ShadyGui::PaletteEditor::onPalettePreview(int index, int32_t color) {
    ShadyCore::Palette* palette = (ShadyCore::Palette*) resource;
    palette->setColor(index, color);
    canvasNormal.setPalette(palette);
    packed.setColor(index, ShadyCore::Palette::unpackColor(ShadyCore::Palette::packColor(color)));
    canvasPacked.setPalette(&packed);
}

void ShadyGui::PaletteEditor::onPaletteConfirm(int index, int32_t color) {
    onPalettePreview(index, color);
    isModified = true;
}
