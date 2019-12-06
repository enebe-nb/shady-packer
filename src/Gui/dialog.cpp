#include "dialog.hpp"

void ShadyGui::LongTaskDialog::taskThread(LongTaskDialog* self) {
    self->running = true;
    self->runTask();
    self->running = false;
    self->notify();
}

ShadyGui::LongTaskDialog::LongTaskDialog(const Glib::ustring& name, bool modal) : Gtk::Dialog(name, modal) {
    auto container = get_content_area();
    container->add(label);
    container->add(progress);
    dispatcher.connect(sigc::mem_fun(*this, &LongTaskDialog::on_notify));
    show_all_children();
}

void ShadyGui::LongTaskDialog::on_notify() {
    if (running) {
        mutex.lock();
        label.set_text(curLabel);
        progress.set_fraction(curProgress);
        mutex.unlock();
    } else {
        thread->join();
        delete thread;
        response(Gtk::RESPONSE_NONE);
    }
}

void ShadyGui::LongTaskDialog::on_show() {
    Gtk::Dialog::on_show();
    if (!running) {
        thread = new std::thread(taskThread, this);
    }
}

void ShadyGui::SavePackageTask::runTask() {
    auto callback = (void(*)(void*, const char*, unsigned int, unsigned int)) &SavePackageTask::callback;
    auto filters |= ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION;
	switch (mode) {
	case ShadyCore::Package::DATA_MODE:
		filters |= ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL;
		filters |= ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH;
		break;
	case ShadyCore::Package::DIR_MODE:
		filters |= ShadyCore::PackageFilter::FILTER_DECRYPT_ALL;
		break;
	case ShadyCore::Package::ZIP_MODE:
		filters |= ShadyCore::PackageFilter::FILTER_TO_ZIP_CONVERTER;
		filters |= ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION;
		filters |= ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE;
		break;
	}
	ShadyCore::PackageFilter::apply(package, filters, -1, callback, this);
	package.save(target.c_str(), mode, callback, this);
}

void ShadyGui::SavePackageTask::callback(SavePackageTask* self, const char* filename, unsigned int index, unsigned int length) {
    self->mutex.lock();
    self->curLabel = filename;
    self->curProgress = index / (float) length;
    self->mutex.unlock();
    self->notify();
}