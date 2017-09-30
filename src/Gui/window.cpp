#include "window.hpp"
#include "application.hpp"
#include "dialog.hpp"

#include <tinyfiledialogs.h>

void ShadyGui::MainWindow::draw(nk_context* context, int width, int height) {
	if (nk_begin(context, "MainWindow", nk_rect(0, 0, width, height), NK_WINDOW_BACKGROUND | NK_WINDOW_NO_SCROLLBAR | (application->isDialogOpen() ? NK_WINDOW_ROM : 0))) {
		nk_layout_row_template_begin(context, 26);
		nk_layout_row_template_push_static(context, 160);
		nk_layout_row_template_push_static(context, 160);
		nk_layout_row_template_push_static(context, 160);
		nk_layout_row_template_push_dynamic(context);
		nk_layout_row_template_end(context);
		if (nk_button_label(context, "Append Folder")) {
			const char * result = tinyfd_selectFolderDialog("Select a Folder", lastPath);
			if (result) package.appendPackage(result);
		} if (nk_button_label(context, "Append Package")) {
			const char * pattern[] = { "*.dat", "*.zip" };
			const char * result = tinyfd_openFileDialog("Select an Archive", lastPath, 2, pattern, 0, 0);
			if (result) package.appendPackage(result);
		} if (nk_button_label(context, "Clear Files")) {
			package.clear();
		} nk_spacing(context, 1);

		if (!package.empty()) {
			if (nk_button_label(context, "Save into Folder")) {
				const char * result = tinyfd_selectFolderDialog("Save to Path", "");
				if (result) application->open(new SavePackageTask(application, result, ShadyCore::Package::DIR_MODE));
			} if (nk_button_label(context, "Save as Data Package")) {
				const char * pattern[] = { "*.dat" };
				const char * result = tinyfd_saveFileDialog("Save Package", "", 1, pattern, 0);
				if (result) application->open(new SavePackageTask(application, result, ShadyCore::Package::DATA_MODE));
			} if (nk_button_label(context, "Save as Zip Package")) {
				const char * pattern[] = { "*.dat","*.zip" };
				const char * result = tinyfd_saveFileDialog("Save Package", "", 2, pattern, 0);
				if (result) application->open(new SavePackageTask(application, result, ShadyCore::Package::ZIP_MODE));
			} nk_edit_string_zero_terminated(context, NK_EDIT_FIELD, searchInput, 256, 0);
		}

		nk_layout_row_dynamic(context, height - 86, 1);
		nk_group_begin(context, "MainGroup", NK_WINDOW_SCROLL_AUTO_HIDE);

		nk_layout_row_template_begin(context, 14);
		nk_layout_row_template_push_static(context, 50);
		nk_layout_row_template_push_static(context, 40);
		nk_layout_row_template_push_dynamic(context);
		nk_layout_row_template_end(context);
		{ std::lock_guard<ShadyCore::Package> lock(package);
		for (ShadyCore::BasePackageEntry& entry : package) {
			if (!strstr(entry.getName(), searchInput)) continue;
			const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);
			if (
				type == ShadyCore::FileType::TYPE_IMAGE
				|| type == ShadyCore::FileType::TYPE_SFX
				|| type == ShadyCore::FileType::TYPE_PALETTE
				|| type == ShadyCore::FileType::TYPE_LABEL) {
				
				if (nk_button_label(context, "open")) application->open(new EditorWindow(application, entry));
			} else nk_spacing(context, 1);
			nk_label(context,
				entry.getType() == ShadyCore::BasePackageEntry::TYPE_FILE ? "[FSY]"
				: entry.getType() == ShadyCore::BasePackageEntry::TYPE_DATA ? "[DAT]"
				: entry.getType() == ShadyCore::BasePackageEntry::TYPE_ZIP ? "[ZIP]"
				: entry.getType() == ShadyCore::BasePackageEntry::TYPE_STREAM ? "[STM]"
				: "", NK_TEXT_CENTERED);
			nk_label(context, entry.getName(), NK_TEXT_LEFT);
		} }

		nk_group_end(context);
	} nk_end(context);
}
