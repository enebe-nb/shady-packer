#include "dialog.hpp"
#include "application.hpp"

#include <mutex>

static std::mutex mutex;
static unsigned int curProgress = 0, maxProgress = 1;
static char* currentName = 0;

void ShadyGui::DialogWindow::draw(nk_context* context, int width, int height) {
	int w = width, h = height;
	getPreferredSize(w, h);

	if (nk_begin(context, getName(), nk_rect((width - w) / 2, (height - h) / 2, w, h), NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		onGuiLayout(context);
	} nk_end(context);
}

static void taskCallback(const char* name, unsigned int cur, unsigned int max) {
	mutex.lock();
	if (currentName) delete[] currentName;
	currentName = new char[strlen(name) + 1];
	strcpy(currentName, name);
	curProgress = cur;
	maxProgress = max;
	mutex.unlock();
}

void ShadyGui::LongTaskDialog::onGuiLayout(nk_context* context) {
	if (!running) {
		running = true;
		std::thread(taskThread, this).detach();
	}

	mutex.lock();
	if (currentName) {
		nk_layout_row_dynamic(context, 20, 1);
		nk_prog(context, curProgress, maxProgress, 0);
		nk_layout_row_dynamic(context, 12, 1);
		nk_label(context, currentName, NK_TEXT_LEFT);
	} mutex.unlock();
	return;
}

void ShadyGui::SavePackageTask::run() {
	ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION, taskCallback);
	switch (mode) {
	case ShadyCore::Package::DATA_MODE:
		ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL, taskCallback);
		ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH, taskCallback);
		break;
	case ShadyCore::Package::DIR_MODE:
		ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_DECRYPT_ALL, taskCallback);
		break;
	case ShadyCore::Package::ZIP_MODE:
		ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_TO_ZIP_CONVERTER, taskCallback);
		ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION, taskCallback);
		ShadyCore::PackageFilter::apply(application->getPackage(), ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE, taskCallback);
		break;
	}
	application->getPackage().save(target.string().c_str(), mode, taskCallback);
}

void ShadyGui::ConfirmDialog::onGuiLayout(nk_context* context) {
	nk_layout_row_dynamic(context, 40, 1);
	nk_label_wrap(context, message);
	
	nk_layout_row_template_begin(context, 26);
	nk_layout_row_template_push_dynamic(context);
	nk_layout_row_template_push_static(context, 50);
	nk_layout_row_template_push_static(context, 50);
	nk_layout_row_template_end(context);
	nk_spacing(context, 1);
	if (nk_button_label(context, "Yes")) {
		callback(userdata, true);
		closed = true;
	} else if (nk_button_label(context, "No")) {
		callback(userdata, false);
		closed = true;
	}
}