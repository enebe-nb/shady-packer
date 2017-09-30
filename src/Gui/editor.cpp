#include "editor.hpp"
#include "dialog.hpp"
#include "application.hpp"

#include <tinyfiledialogs.h>

#define NK_EDITORWINDOW_FLAGS \
	NK_WINDOW_BORDER | \
	NK_WINDOW_MOVABLE | \
	NK_WINDOW_SCALABLE | \
	NK_WINDOW_CLOSABLE | \
	NK_WINDOW_TITLE | \
	NK_WINDOW_SCROLL_AUTO_HIDE

static int editorNameCounter = 0;

ShadyGui::EditorWindow::EditorWindow(Application* application, ShadyCore::BasePackageEntry& entry)
	: Window(application)
	, type(ShadyCore::FileType::get(entry)) {
	sprintf(name, "Editor%08x", editorNameCounter++);
	switch (type.type) {
		case ShadyCore::FileType::TYPE_IMAGE: component = new ImageEditor; break;
		case ShadyCore::FileType::TYPE_LABEL: component = new LabelEditor; break;
		case ShadyCore::FileType::TYPE_SFX: component = new AudioEditor; break;
		//case ShadyCore::FileType::TYPE_TEXT: component = new TextEditor; break;
		case ShadyCore::FileType::TYPE_PALETTE: component = new PaletteEditor; break;
		default: throw; // TODO Unsupported
	}

	component->application = application;
	component->resource = ShadyCore::readResource(type, entry.open()); entry.close();
	component->path = entry.getName();
	component->initialize();
	if (!component->resource) throw; // TODO better handling
}

ShadyGui::EditorWindow::~EditorWindow() { delete component; }

void ShadyGui::EditorWindow::draw(nk_context* context, int width, int height) {
	int w = width, h = height;
	component->getPreferredSize(w, h);

	if (nk_begin_titled(context, getName(), component->path, nk_rect((width - w) / 2, (height - h) / 2, w, h)
		, NK_EDITORWINDOW_FLAGS | (application->isDialogOpen() ? NK_WINDOW_ROM : 0))) {
		component->onGuiLayout(context);
		
		nk_layout_row_dynamic(context, 20, 3);
		if (nk_button_label(context, "Import")) {
			const char* pattern[] = { type.normalExt, type.inverseExt };
			const char * result = tinyfd_openFileDialog("Load a Resource File", "", 2, pattern, 0, 0);
			if (result) {
				std::ifstream input(result, std::ios::binary);
				bool isNormal = strcmp(type.normalExt, result + strlen(result) - strlen(type.normalExt)) == 0;
				ShadyCore::readResource(component->resource, input, isNormal == type.isEncrypted);
				component->isModified = true;
				component->initialize();
			}
		}
		if (nk_button_label(context, "Export")) {
			const char* pattern[] = { type.normalExt, type.inverseExt };
			const char* result = tinyfd_saveFileDialog("Export a Resource File", "", 2, pattern, 0);
			if (result) {
				std::ofstream output(result, std::ios::binary);
				bool isNormal = strcmp(type.normalExt, result + strlen(result) - strlen(type.normalExt)) == 0;
				ShadyCore::writeResource(component->resource, output, isNormal == type.isEncrypted);
				// TODO verify extension
			}
		} if (component->isModified) if (nk_button_label(context, "Save")) {
			const ShadyCore::FileType& type = ShadyCore::FileType::get(*application->getPackage().findFile(component->path));
			boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
			std::ofstream output(tempFile.native(), std::ios::binary);
			ShadyCore::writeResource(component->resource, output, type.isEncrypted);
			output.close();

			application->getPackage().appendFile(component->path, tempFile.generic_string().c_str(), true);
			application->setPackageModified(true);
		}

		nk_end(context);
	} else {
		nk_end(context);
		if (component->isModified) {
			nk_window_show(context, getName(), NK_SHOWN);
			application->open(new ConfirmDialog(application, this, (ConfirmDialog::Callback)&closeCallback
				, "Unsaved modifications", "This editor has unsaved modifcations, do you want discard them?"));
		} else closed = true;
	}

	if (closed) nk_window_close(context, getName());
}

ShadyGui::EditorWindow::EditorComponent::~EditorComponent() { delete resource; }
ShadyGui::ImageEditor::~ImageEditor() { if (renderer.getPalette()) delete renderer.getPalette(); }

static bool stepResource(ShadyCore::Resource* resource, const char*& path, ShadyCore::Package& package, ShadyCore::FileType::Type type, bool forward) {
	for (auto iter = package.findFile(path); forward ? ++iter != package.end() : iter-- != package.begin();) {
		const ShadyCore::FileType& iterType = ShadyCore::FileType::get(*iter);
		if (iterType == type) {
			ShadyCore::readResource(resource, iter->open(), iterType.isEncrypted); iter->close();
			path = iter->getName();
			return true;
		}
	} return false;
}

static ShadyCore::Palette* loadPalette(ShadyCore::Package& package, const char* path, int index) {
	char buffer[256];

	const char* e = strrchr(path, '/');
	if (!e) e = strrchr(path, '_');
	if (!e) return 0;

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

static ShadyCore::Image* loadImage(ShadyCore::Package& package, const char* path, int& index) {
	const char* e = strrchr(path, '/');
	if (!e) e = strrchr(path, '_');
	if (!e) return 0;

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

void ShadyGui::ImageEditor::setPalette(ShadyCore::Palette* palette) {
	if (palette) {
		if (renderer.getPalette()) delete renderer.getPalette();
		renderer.setPalette(palette);
	}
}

void ShadyGui::ImageEditor::getPreferredSize(int& width, int& height) const {
	width = std::min((unsigned)width, ((ShadyCore::Image*)resource)->getWidth() + 22);
	height = std::min((unsigned)height, ((ShadyCore::Image*)resource)->getHeight() + 76);
}

void ShadyGui::ImageEditor::initialize() {
	renderer.setImage(dynamic_cast<ShadyCore::Image*>(resource));
	
	if (((ShadyCore::Image*)resource)->getBitsPerPixel() == 8) {
		setPalette(loadPalette(application->getPackage(), path, curPalette));
	}
}

void ShadyGui::ImageEditor::onGuiLayout(nk_context* context) {
	if (!isModified) if (nk_input_is_key_pressed(&context->input, NK_KEY_UP)) {
		setPalette(loadPalette(application->getPackage(), path, --curPalette < 0 ? (curPalette = 7) : curPalette));
	} else if (nk_input_is_key_pressed(&context->input, NK_KEY_DOWN)) {
		setPalette(loadPalette(application->getPackage(), path, ++curPalette > 7 ? (curPalette = 0) : curPalette));
	} else if (nk_input_is_key_pressed(&context->input, NK_KEY_LEFT)) {
		stepResource(resource, path, application->getPackage(), ShadyCore::FileType::TYPE_IMAGE, false);
		renderer.setImage((ShadyCore::Image*)resource);
		if (((ShadyCore::Image*)resource)->getBitsPerPixel() == 8) setPalette(loadPalette(application->getPackage(), path, curPalette));
	} else if (nk_input_is_key_pressed(&context->input, NK_KEY_RIGHT)) {
		stepResource(resource, path, application->getPackage(), ShadyCore::FileType::TYPE_IMAGE, true);
		renderer.setImage((ShadyCore::Image*)resource);
		if (((ShadyCore::Image*)resource)->getBitsPerPixel() == 8) setPalette(loadPalette(application->getPackage(), path, curPalette));
	}

	nk_layout_row_dynamic(context, nk_window_get_height(context) - 76, 1);
	if (nk_window_has_focus(context)
		&& context->input.mouse.buttons[NK_BUTTON_LEFT].down
		&& nk_input_has_mouse_click_down_in_rect(&context->input, NK_BUTTON_LEFT, nk_window_get_content_region(context), true)
		&& nk_widget_has_mouse_click_down(context, NK_BUTTON_LEFT, true)) {

		renderer.setPosition(renderer.getPositionX() + context->input.mouse.delta.x, renderer.getPositionY() + context->input.mouse.delta.y);
	}
	
	renderer.draw(context);
}

void ShadyGui::AudioEditor::initialize() {
	renderer.setAudio(dynamic_cast<ShadyCore::Sfx*>(resource));
}

void ShadyGui::AudioEditor::onGuiLayout(nk_context* context) {
	nk_layout_row_template_begin(context, 20);
	nk_layout_row_template_push_static(context, 50);
	nk_layout_row_template_end(context);

	if (nk_button_label(context, "Play")) {
		renderer.play();
	}
}

void ShadyGui::LabelEditor::initialize() {
	strcpy(inputName, ((ShadyCore::LabelResource*)resource)->getName());
	sprintf(inputOffset, "%f", ((ShadyCore::LabelResource*)resource)->getOffset() / 44100.f);
	sprintf(inputSize, "%f", ((ShadyCore::LabelResource*)resource)->getSize() / 44100.f);
}

void ShadyGui::LabelEditor::onGuiLayout(nk_context* context) {
	nk_layout_row_template_begin(context, 20);
	nk_layout_row_template_push_static(context, 80);
	nk_layout_row_template_push_dynamic(context);
	nk_layout_row_template_end(context);

	nk_label(context, "Name:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(context, NK_EDIT_READ_ONLY, inputName, 255, 0);
	
	nk_label(context, "Loop Start:", NK_TEXT_LEFT);
	if (nk_edit_string_zero_terminated(context, NK_EDIT_FIELD, inputOffset, 255, nk_filter_float) & (NK_EDIT_ACTIVE | NK_EDIT_COMMITED)) {
		((ShadyCore::LabelResource*)resource)->setOffset(atof(inputOffset) * 44100.f);
		isModified = true;
	}
	
	nk_label(context, "Loop Dur.:", NK_TEXT_LEFT);
	if (nk_edit_string_zero_terminated(context, NK_EDIT_FIELD, inputSize, 255, nk_filter_float) & (NK_EDIT_ACTIVE | NK_EDIT_COMMITED)) {
		((ShadyCore::LabelResource*)resource)->setSize(atof(inputSize) * 44100.f);;
		isModified = true;
	}
}

/*
ShadyGui::TextEditor::~TextEditor() { if (initialized) nk_textedit_free(&input); }

void ShadyGui::TextEditor::initialize() {
	if (initialized) nk_textedit_free(&input);
	
	nk_textedit_init_default(&input);
	nk_str_append_text_utf8(&input.string, ((ShadyCore::TextResource*)resource)->getData(), ((ShadyCore::TextResource*)resource)->getLength());

	initialized = true;
}

void ShadyGui::TextEditor::onGuiLayout(nk_context* context) {
	nk_layout_row_dynamic(context, nk_window_get_height(context) - 76, 1);
	if (nk_edit_buffer(context, NK_EDIT_EDITOR, &input, 0) & (NK_EDIT_ACTIVE | NK_EDIT_COMMITED)) {
		int len = nk_str_len_char(&input.string);
		((ShadyCore::TextResource*)resource)->initialize(len);
		memcpy(((ShadyCore::TextResource*)resource)->getData(), nk_str_get_const(&input.string), len);
	}
}
*/

ShadyGui::PaletteEditor::~PaletteEditor() { if (renderNormal.getImage()) delete renderNormal.getImage(); }

void ShadyGui::PaletteEditor::setImage(ShadyCore::Image* image) {
	if (image) {
		if (renderNormal.getImage()) delete renderNormal.getImage();
		renderNormal.setImage(image);
		renderPacked.setImage(image);
	}
}

void ShadyGui::PaletteEditor::initialize() {
	renderNormal.setPalette(dynamic_cast<ShadyCore::Palette*>(resource));
	curColor = nk_rgba_u32(((ShadyCore::Palette*)resource)->getColor(curColorIndex));

	for (int i = 0; i < 256; ++i) {
		packed.setColor(i, ShadyCore::Palette::unpackColor(ShadyCore::Palette::packColor(((ShadyCore::Palette*)resource)->getColor(i))));
	} renderPacked.setPalette(&packed);

	setImage(loadImage(application->getPackage(), path, curImage));
}

void ShadyGui::PaletteEditor::onGuiLayout(nk_context* context) {
	if (nk_input_is_key_pressed(&context->input, NK_KEY_LEFT)) {
		setImage(loadImage(application->getPackage(), path, --curImage));
	} else if (nk_input_is_key_pressed(&context->input, NK_KEY_RIGHT)) {
		setImage(loadImage(application->getPackage(), path, ++curImage));
	}
	
	nk_layout_row_template_begin(context, nk_window_get_height(context) - 76);
	nk_layout_row_template_push_dynamic(context);
	nk_layout_row_template_push_dynamic(context);
	nk_layout_row_template_push_static(context, 177);
	nk_layout_row_template_end(context);

	if (nk_window_has_focus(context)
		&& context->input.mouse.buttons[NK_BUTTON_LEFT].down
		&& nk_input_has_mouse_click_down_in_rect(&context->input, NK_BUTTON_LEFT, nk_window_get_content_region(context), true)
		&& nk_widget_has_mouse_click_down(context, NK_BUTTON_LEFT, true)) {

		renderNormal.setPosition(renderNormal.getPositionX() + context->input.mouse.delta.x, renderNormal.getPositionY() + context->input.mouse.delta.y);
	}
	renderNormal.draw(context);

	if (nk_window_has_focus(context)
		&& context->input.mouse.buttons[NK_BUTTON_LEFT].down
		&& nk_input_has_mouse_click_down_in_rect(&context->input, NK_BUTTON_LEFT, nk_window_get_content_region(context), true)
		&& nk_widget_has_mouse_click_down(context, NK_BUTTON_LEFT, true)) {

		renderPacked.setPosition(renderPacked.getPositionX() + context->input.mouse.delta.x, renderPacked.getPositionY() + context->input.mouse.delta.y);
	}
	renderPacked.draw(context);

	nk_group_begin(context, "Options Group", 0);
	nk_layout_row_dynamic(context, 150, 1);

	{ nk_command_buffer *canvas = nk_window_get_canvas(context);
		struct nk_rect bounds;
		
		if (nk_widget(&bounds, context)) {
			int stepW = bounds.w / 16, stepH = bounds.h / 16;
			for (int i = 0; i < 16; ++i) for (int j = 0; j < 16; ++j) {
				nk_fill_rect(canvas, nk_rect((stepW * i) + bounds.x, (stepH * j) + bounds.y, stepW, stepH), 0, nk_rgba_u32(((ShadyCore::Palette*)resource)->getColor(j * 16 + i)));
			}

			int i = curColorIndex % 16, j = curColorIndex / 16;
			nk_stroke_rect(canvas, nk_rect((stepW * i) + bounds.x, (stepH * j) + bounds.y, stepW, stepH), 0, 2, nk_rgb(255, 0, 255));
		}

		if (nk_window_has_focus(context)
			&& nk_input_has_mouse_click_down_in_rect(&context->input, NK_BUTTON_LEFT, nk_window_get_content_region(context), true)
			&& nk_input_is_mouse_click_down_in_rect(&context->input, NK_BUTTON_LEFT, bounds, true)) {
			int stepW = bounds.w / 16, stepH = bounds.h / 16;
			curColorIndex =
				(int)((context->input.mouse.pos.y - bounds.y) / stepH) * 16 +
				(int)((context->input.mouse.pos.x - bounds.x) / stepW);
		}
	}

	if (nk_color_pick(context, &curColor, NK_RGB)) {
		uint32_t color = nk_color_u32(curColor);
		((ShadyCore::Palette*)resource)->setColor(curColorIndex, color);
		packed.setColor(curColorIndex, ShadyCore::Palette::unpackColor(ShadyCore::Palette::packColor(color)));

		renderNormal.setPalette((ShadyCore::Palette*)resource);
		renderPacked.setPalette(&packed);
		isModified = true;
	}
	nk_group_end(context);
}
