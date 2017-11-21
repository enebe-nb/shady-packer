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
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION, callback, this);
	switch (mode) {
	case ShadyCore::Package::DATA_MODE:
		ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL, callback, this);
		ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH, callback, this);
		break;
	case ShadyCore::Package::DIR_MODE:
		ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_DECRYPT_ALL, callback, this);
		break;
	case ShadyCore::Package::ZIP_MODE:
		ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_ZIP_CONVERTER, callback, this);
		ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION, callback, this);
		ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE, callback, this);
		break;
	}
	package.save(target.c_str(), mode, callback, this);
}

void ShadyGui::SavePackageTask::callback(SavePackageTask* self, const char* filename, unsigned int index, unsigned int length) {
    self->mutex.lock();
    self->curLabel = filename;
    self->curProgress = index / (float) length;
    self->mutex.unlock();
    self->notify();
}