#include "main.hpp"
#include "modpackage.hpp"
#include "menu.hpp"

namespace {
	ModPackage* selectedPackage = 0;
	std::list<ModPackage*> downloads;
	FetchJson remoteConfig;
}

static void setPackageEnabled(ModPackage* package, bool value) {
	if (package->enabled == value) return;
	package->enabled = value;
	if (package->enabled) {
		EnablePackage(package);
	} else {
		DisablePackage(package);
	}
}

namespace{
	class : public AsyncTask {
	protected:
		void run() {
			using namespace std::chrono_literals;

			while (!downloads.empty()) {
				for (auto i = downloads.begin(); i != downloads.end();) {
					if ((*i)->downloadTask && (*i)->downloadTask->isDone()) {
						ModPackage* package = (*i);
						bool wasEnabled = package->enabled;
						if (wasEnabled) {
							if (iniUseLoadLock) loadLock.lock();
							setPackageEnabled(package, false);
						}
						std::filesystem::path filename(ModPackage::basePath / package->name);
						filename += package->ext; filename += L".part";
						if (std::filesystem::exists(filename)) {
							std::filesystem::path target(filename);
							target.replace_extension();
							std::error_code err;
							std::filesystem::remove(target, err);
							if (!err) std::filesystem::rename(filename, target, err);
							if (!err) {
								package->data["version"] = package->data.value("remoteVersion", "");
								package->requireUpdate = false;
								package->fileExists = true;
							}
						}
 						delete package->downloadTask;
						package->downloadTask = 0;
						if (wasEnabled) {
							setPackageEnabled(package, true);
							if (iniUseLoadLock) loadLock.unlock();
						}
						SaveSettings();
						i = downloads.erase(i);
					} else ++i;
				}
				std::this_thread::sleep_for(1000ms);
			}
		}
	} downloadController; // TODO can't reuse Tasks

	class : public AsyncTask {
	protected:
		void run() {
			using namespace std::chrono_literals;
			// Wait for module database
			while (true) {
				if (remoteConfig.isDone()) {
					// We are sync here
					if (!remoteConfig.data.is_object()) return;
					ModPackage::LoadFromRemote(remoteConfig.data);
					if (iniAutoUpdate) for (auto& package : ModPackage::packageList) {
						if (!package->isLocal() && package->requireUpdate) {
							package->downloadFile();
							downloads.push_back(package);
						}
					}
					break;
				}
				std::this_thread::sleep_for(100ms);
			}

			downloadController.start();
		}
	} updateController;

}

ModList::ModList() : CFileList() {
	this->maxLength = 26;
	this->extLength = 0;
}

void ModList::renderScroll(float x, float y, int offset, int size, int view) {
	// just set values, render is done on CDesign
	this->scrollLen = size < view ? 286 : view*286/size;
	this->scrollBar->y1 = 286*offset/size + this->scrollLen - 286;

	for (int i = 0; i < view && i < size - offset; ++i) {
		this->renderLine(x, y + i*16, i + offset);
	}
}

void ModList::updateList() {
	this->names.clear();
	this->types.clear();
	for (auto package : ModPackage::packageList) {
		auto name = package->name.string();
		SokuLib::String str; str.assign(name.c_str(), name.length());
		this->names.push_back(str);
		this->types.push_back(package->enabled);
	}
	this->updateResources();
}

int ModList::appendLine(SokuLib::String& out, void* unknown, SokuLib::Deque<SokuLib::String>& list, int index) {
	ModPackage* package = ModPackage::packageList[index];
	int color = package->enabled ? 0x40ff40
		: package->isLocal() ? 0x6060d0
		: 0x808080;

	char buffer[15]; sprintf(buffer, "<color %06x>", color);
	out.append(buffer, 14);
	int len = CFileList::appendLine(out, unknown, list, index);
	out.append("</color>", 8);

	return len;
}

ModMenu::ModMenu() {
	design.loadResource("shady/downloader.dat");
	modList.updateList();

	design.getById((SokuLib::CDesign::Sprite**)&modList.scrollBar, 101);
	modList.scrollBar->active = true;
	modList.scrollBar->gauge.set(&modList.scrollLen, 0, 286);
	modCursor.set(&SokuLib::inputMgrs.input.verticalAxis, modList.names.size());
	this->updateView(modCursor.pos);

	if (!remoteConfig.isRunning() && !remoteConfig.isDone()) {
		remoteConfig.fileId = iniRemoteConfig;
		remoteConfig.start();
		syncing = true;
	}
}

ModMenu::~ModMenu() {
	design.clear();
	modList.clear();
}

void ModMenu::_() {}

int ModMenu::onProcess() {
	if (syncing && remoteConfig.isDone()) {
		syncing = false;
		if (remoteConfig.data.is_object()) ModPackage::LoadFromRemote(remoteConfig.data);
		modList.updateList();
		modCursor.set(&SokuLib::inputMgrs.input.verticalAxis, modList.names.size(), modCursor.pos);
		this->updateView(modCursor.pos);
	}

	// Cursor On List
	if (this->state == 0) {
		if (modCursor.update()) {
			SokuLib::playSEWaveBuffer(0x27);
			this->updateView(modCursor.pos);
		}

		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			return false; // close
		}

		if (SokuLib::inputMgrs.input.a == 1 && this->options != 3) {
			SokuLib::playSEWaveBuffer(0x28);
			this->state = 1;
			viewCursor.set(&SokuLib::inputMgrs.input.verticalAxis, this->options == 1 ? 2 : 1);
		}
	// Cursor On View
	} else if (this->state == 1) {
		if (viewCursor.update()) {
			SokuLib::playSEWaveBuffer(0x27);
		}

		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			this->state = 0;
			return true;
		}

		if (SokuLib::inputMgrs.input.a == 1) {
			ModPackage* package = ModPackage::packageList[modCursor.pos];
			if (options < 2 && viewCursor.pos == 0) {
				SokuLib::playSEWaveBuffer(0x28);
				setPackageEnabled(package, !package->enabled);
				modList.updateList();
				this->updateView(modCursor.pos);
			} else if (options != 3) {
				SokuLib::playSEWaveBuffer(0x28);
				package->downloadFile();
				downloads.push_back(package);
				if (!downloadController.isRunning()) downloadController.start();
				this->updateView(modCursor.pos);
				this->state = 0;
			}
		}
	}

	return !SokuLib::checkKeyOneshot(0x1, false, false, false);
}

int ModMenu::onRender() {
	design.render4();

	if (modCursor.pos > scrollPos + 16) {
		scrollPos = modCursor.pos - 16;
	} else if (modCursor.pos < scrollPos) {
		scrollPos = modCursor.pos;
	}

	SokuLib::CDesign::Object* pos;
	design.getById(&pos, 100);
	if (this->state == 0) SokuLib::MenuCursor::render(pos->x2, pos->y2 + (modCursor.pos - scrollPos)*16, 256);
	modList.renderScroll(pos->x2, pos->y2, scrollPos, modList.getLength(), 17);

	design.getById(&pos, 200);
	viewTitle.render(pos->x2, pos->y2);
	design.getById(&pos, 201);
	viewContent.render(pos->x2, pos->y2);
	design.getById(&pos, 202);
	if (this->state == 1 && this->options != 3) SokuLib::MenuCursor::render(pos->x2, pos->y2 + viewCursor.pos*16, 256);
	viewOption.render(pos->x2, pos->y2);

	return 0;
}

void ModMenu::updateView(int index) {
	ModPackage* package = ModPackage::packageList[index];
	SokuLib::FontDescription fontDesc {
		"",
		0xff, 0xa0, 0xff, 0xa0, 0xff, 0xff,
		20, 400,
		false, true, false,
		100000, 0, 0, 0, 2
	}; strcpy(fontDesc.faceName, SokuLib::defaultFontName);
	SokuLib::SWRFont font; font.create();
	int textureId;

	font.setIndirect(fontDesc);
	SokuLib::textureMgr.createTextTexture(&textureId, package->name.string().c_str(), font, 220, 24, 0, 0);
	viewTitle.setTexture2(textureId, 0, 0, 220, 24);

	fontDesc.weight = 300;
	fontDesc.height = 14;
	fontDesc.useOffset = true;
	font.setIndirect(fontDesc);
	std::string temp;
	if (package->isLocal()) temp = "<color 404040>This is a local Package.</color>";
	else {
		temp += "Version: <color 404040>" + package->version() + "</color><br>";
		temp += "Creator: <color 404040>" + package->creator() + "</color><br>";
		temp += "Description: <color 404040>" + package->description() + "</color><br>";
		temp += "Tags: ";
		for (int i = 0; i < package->tags.size(); ++i) {
			if (i > 0) temp += ", ";
			temp += "<color 404040>" + package->tags[i] + "</color>";
		}
	}
	// TODO status
	SokuLib::textureMgr.createTextTexture(&textureId, temp.c_str(), font, 220, 190, 0, 0);
	viewContent.setTexture2(textureId, 0, 0, 220, 190);

	if (package->downloadTask) {
		this->options = 3;
		temp = "Downloading ...";
	} else if (package->fileExists) {
		temp = (package->enabled ? "Disable Package<br>" : "Enable Package<br>");
		if (package->requireUpdate) {
			this->options = 1;
			temp += "Update Package<br>";
		} else this->options = 0;
	} else {
		this->options = 2;
		temp = "Download Package<br>";
	}
	SokuLib::textureMgr.createTextTexture(&textureId, temp.c_str(), font, 220, 40, 0, 0);
	viewOption.setTexture2(textureId, 0, 0, 220, 40);
}

void LoadPackage() {
	ModPackage::LoadFromLocalData();
	ModPackage::LoadFromFilesystem();

	for (auto& package : ModPackage::packageList) {
		bool isEnabled = GetPrivateProfileIntW(L"Packages", package->name.c_str(),
			false, (ModPackage::basePath / L"shady-loader.ini").c_str());
		setPackageEnabled(package, isEnabled);
	}
}

void UnloadPackage() {
	loadLock.lock();
	for (auto& package : ModPackage::packageList) {
		setPackageEnabled(package, false);
		delete package;
	}
	loadLock.unlock();
	ModPackage::packageList.clear();
}

void StartUpdate() {
	remoteConfig.fileId = iniRemoteConfig;
	remoteConfig.start();
	updateController.start();
}