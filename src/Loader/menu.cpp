#include "main.hpp"
#include "modpackage.hpp"
#include "menu.hpp"
#include "../Core/util/filewatcher.hpp"
#include "th123intl.hpp"
#include <mbstring.h>

namespace {
	ModPackage* selectedPackage = 0;
	std::list<ModPackage*> downloads;
	_locale_t intlLocale;

	enum :int { OPTION_ENABLE_DISABLE, OPTION_DOWNLOAD, OPTION_SHOW };
}

static void setPackageEnabled(ModPackage* package, bool value) {
	if (package->isEnabled() == value) return;
	if (value) {
		EnablePackage(package);
		package->watcher = ShadyUtil::FileWatcher::create(package->path);
	} else {
		DisablePackage(package);
		if (package->watcher) {
			delete package->watcher;
			package->watcher = 0;
		}
	}
}

static const char* translateExecuteError(int val) {
	switch (val) {
		case 0: return "The operating system is out of memory or resources.";
		case 1: return "The file is invalid or does not exists";
		case ERROR_FILE_NOT_FOUND: return "The specified file was not found.";
		case ERROR_PATH_NOT_FOUND: return "The specified path was not found.";
		case ERROR_BAD_FORMAT: return "The .exe file is invalid (non-Win32 .exe or error in .exe image).";
		case SE_ERR_ACCESSDENIED: return "The operating system denied access to the specified file.";
		case SE_ERR_ASSOCINCOMPLETE: return "The file name association is incomplete or invalid.";
		case SE_ERR_DDEBUSY: return "The DDE transaction could not be completed because other DDE transactions were being processed.";
		case SE_ERR_DDEFAIL: return "The DDE transaction failed.";
		case SE_ERR_DDETIMEOUT: return "The DDE transaction could not be completed because the request timed out.";
		case SE_ERR_DLLNOTFOUND: return "The specified DLL was not found.";
		case SE_ERR_NOASSOC: return "There is no application associated with the given file name extension.";
		case SE_ERR_OOM: return "There was not enough memory to complete the operation.";
		case SE_ERR_SHARE: return "A sharing violation occurred.";
		default: return "Unknown Error";
	}
}

ModList::ModList() : CFileList() {
	this->maxLength = 26;
	this->extLength = 0;
}

void ModList::renderScroll(float x, float y, int offset, int size, int view) {
	size = size == 0 ? 1 : size;
	// just set values, render is done on CDesign
	this->scrollLen = size < view ? 286 : view*286/size;
	this->scrollBar->y1 = 286*offset/size + this->scrollLen - 286;

	for (int i = 0; i < view && i < size - offset; ++i) {
		this->renderLine(x, y + i*16, i + offset);
	}
}

void ModList::updateList() {
	auto locale = th123intl::GetLocale<intlLocale>();
	auto cp = th123intl::GetTextCodePage();
	this->names.clear();
	this->types.clear();
	for (auto package : ModPackage::descPackage) {
		std::string name; th123intl::ConvertCodePage(CP_UTF8, package->name, cp, name);
		if (this->maxLength) {
			auto charCount = _mbsnccnt_l((uint8_t*)name.c_str(), this->maxLength-2, locale);
			name.resize(_mbsnbcnt_l((uint8_t*)name.c_str(), charCount, locale));
		}
		this->names.emplace_back();
		this->names.back().assign(name.c_str(), name.size());
		this->types.push_back(package->isEnabled());
	}
	this->updateResources();
}

static inline int _getPackageColor(ModPackage* package) {
	if (package->requireUpdate) return package->isLocal() ? 0x909020 : 0xff8040;
	if (package->isEnabled()) {
		if (package->package->empty()
			|| package->package->size() == 1 
			&& package->package->find("init.lua") != package->package->end()) return 0x40ff40;
		return 0xff2020;
	} if (package->isLocal()) return 0x6060d0;
	return 0x808080;
}

int ModList::appendLine(SokuLib::String& out, void* unknown, SokuLib::Deque<SokuLib::String>& list, int index) {
	ModPackage* package = ModPackage::descPackage[index];
	int color = _getPackageColor(package);

	char buffer[15]; sprintf(buffer, "<color %06x>", color);
	out.append(buffer, 14);
	int len = CFileList::appendLine(out, unknown, list, index);
	out.append("</color>", 8);

	return len;
}

ModMenu::ModMenu() {
	design.loadResource("shady/downloader.dat");
	ModPackage::LoadFromFilesystem();
	(guide.*SokuLib::union_cast<void (SokuLib::Guide::*)(unsigned int)>(0x443160))(89); // Init
	guide.active = true;

	design.getById((SokuLib::CDesign::Sprite**)&modList.scrollBar, 101);
	modList.scrollBar->active = true;
	modList.scrollBar->gauge.set(&modList.scrollLen, 0, 286);
	modCursor.set(&SokuLib::inputMgrs.input.verticalAxis, modList.names.size());

	ModPackage::LoadFromRemote();
}

ModMenu::~ModMenu() {
	design.clear();
	modList.clear();
	if (viewTitle.dxHandle) SokuLib::textureMgr.remove(viewTitle.dxHandle);
	viewTitle.dxHandle = 0;
	if (viewContent.dxHandle) SokuLib::textureMgr.remove(viewContent.dxHandle);
	viewContent.dxHandle = 0;
	if (viewOption.dxHandle) SokuLib::textureMgr.remove(viewOption.dxHandle);
	viewOption.dxHandle = 0;
	if (viewPreview.dxHandle) SokuLib::textureMgr.remove(viewPreview.dxHandle);
	viewPreview.dxHandle = 0;
	(guide.*SokuLib::union_cast<void (SokuLib::Guide::*)()>(0x443100))(); // Cleanup
}

void ModMenu::_() {}

int ModMenu::onProcess() {
	(guide.*SokuLib::union_cast<void (SokuLib::Guide::*)()>(0x443220))(); // Update
	if (ModPackage::Notify()) viewDirty = listDirty = true;
	if (ModPackage::descMutex.try_lock_shared()) {
		if (listDirty) {
			modList.updateList();
			modCursor.set(&SokuLib::inputMgrs.input.verticalAxis, modList.names.size(), modCursor.pos);
		}
		if (viewDirty) this->updateView(modCursor.pos);
		ModPackage::descMutex.unlock_shared();
		viewDirty = listDirty = false;
	}

	if (settingsDirty) {
		SaveSettings();
		settingsDirty = false;
	}

	// // Cursor On List
	if (this->state == 0) {
		if (modCursor.update()) {
			SokuLib::playSEWaveBuffer(0x27);
			viewDirty = true;
		}

		if (SokuLib::inputMgrs.input.c == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			if (orderCursor >= 0) {
				this->swap(orderCursor, modCursor.pos);
				orderCursor = -1;
			} else orderCursor = modCursor.pos;
			return true;
		}

		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			if (orderCursor >= 0) {
				orderCursor = -1;
				return true;
			} else return false; // close
		}

		if (SokuLib::inputMgrs.input.a == 1 && this->optionCount) {
			SokuLib::playSEWaveBuffer(0x28);
			if (orderCursor >= 0) {
				this->swap(orderCursor, modCursor.pos);
				orderCursor = -1;
			} else {
				this->state = 1;
				viewCursor.set(&SokuLib::inputMgrs.input.verticalAxis, this->optionCount);
			}
		}

	// Cursor On View
	} else if (this->state == 1 && !hasAction) {
		if (viewCursor.update()) {
			SokuLib::playSEWaveBuffer(0x27);
		}

		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			this->state = 0;
			return true;
		}

		if (SokuLib::inputMgrs.input.a == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			hasAction = true;
		}
	}

	if (hasAction && ModPackage::descMutex.try_lock_shared()) {
		ModPackage* package = ModPackage::descPackage[modCursor.pos];
		switch (options[viewCursor.pos]) {
			case OPTION_ENABLE_DISABLE:
				setPackageEnabled(package, !package->package);
				settingsDirty = listDirty = true;
				break;
			case OPTION_DOWNLOAD:
				package->downloadFile();
				this->state = 0;
				break;
			case OPTION_SHOW:
				HINSTANCE result = (HINSTANCE)1;
				if (std::filesystem::is_directory(package->path)) {
					result = ShellExecuteW(0, L"explore", package->path.c_str(), L"", 0, SW_NORMAL);
				} else if (std::filesystem::is_regular_file(package->path)) {
					std::wstring execPath;
					int len = MAX_PATH + 1;
					do { execPath.resize(len);
						len = ExpandEnvironmentStringsW(L"%windir%\\explorer.exe", execPath.data(), len);
					} while(len > execPath.size());

					result = ShellExecuteW(0, L"open", execPath.c_str(),
						(L"/select,\"" + package->path.wstring() + L"\"").c_str(), 0, SW_NORMAL);
				}

				if (result <= (HINSTANCE)32) {
					char errTitle[32] = "Explorer Error: ";
					itoa(GetLastError(), &errTitle[16], 10);
					MessageBox(0, translateExecuteError((int)result), errTitle, MB_OK);
				}
				break;
		}

		ModPackage::descMutex.unlock_shared();
		viewDirty = true; hasAction = false;
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
	if (this->state == 0) {
		SokuLib::MenuCursor::render(pos->x2, pos->y2 + (modCursor.pos - scrollPos)*16, 256);
		if (orderCursor >= scrollPos && orderCursor <= scrollPos + 16)
			SokuLib::MenuCursor::render(pos->x2, pos->y2 + (orderCursor - scrollPos)*16, 256);
	}
	modList.renderScroll(pos->x2, pos->y2, scrollPos, modList.getLength(), 17);

	design.getById(&pos, 200);
	viewTitle.render(pos->x2, pos->y2);
	design.getById(&pos, 201);
	viewContent.render(pos->x2, pos->y2);
	design.getById(&pos, 202);
	if (this->state == 1) SokuLib::MenuCursor::render(pos->x2, pos->y2 + viewCursor.pos*16, 120);
	viewOption.render(pos->x2, pos->y2);
	design.getById(&pos, 203);
	if(viewPreview.dxHandle) viewPreview.renderScreen(pos->x2, pos->y2, pos->x2 + 200, pos->y2 + 150);

	(guide.*SokuLib::union_cast<void (SokuLib::Guide::*)()>(0x443260))(); // Render
	return 0;
}

void ModMenu::updateView(int index) {
	auto cp = th123intl::GetTextCodePage();
	ModPackage* package = ModPackage::descPackage[index];
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
	{
		std::string name; th123intl::ConvertCodePage(CP_UTF8, package->name, cp, name);
		SokuLib::textureMgr.createTextTexture(&textureId, name.c_str(), font, 330, 24, 0, 0);
	}
	if (viewTitle.dxHandle) SokuLib::textureMgr.remove(viewTitle.dxHandle);
	viewTitle.setTexture2(textureId, 0, 0, 330, 24);

	fontDesc.weight = 300;
	fontDesc.height = 14;
	fontDesc.useOffset = true;
	font.setIndirect(fontDesc);
	std::string temp;
	if (package->isLocal()) temp = "<color 404040>This is a local Package.</color>";
	else {
		std::string cpStr;
		th123intl::ConvertCodePage(CP_UTF8, package->version(), cp, cpStr);
		temp += "Version: <color 606060>" + cpStr + "</color><br>";
		th123intl::ConvertCodePage(CP_UTF8, package->creator(), cp, cpStr);
		temp += "Creator: <color 606060>" + cpStr + "</color><br>";
		th123intl::ConvertCodePage(CP_UTF8, package->description(), cp, cpStr);
		temp += "Description: <color 606060>" + cpStr + "</color><br>";
		temp += "Tags: ";
		for (int i = 0; i < package->tags.size(); ++i) {
			if (i > 0) temp += ", ";
			th123intl::ConvertCodePage(CP_UTF8, package->tags[i], cp, cpStr);
			temp += "<color 606060>" + cpStr + "</color>";
		}
	}

	SokuLib::textureMgr.createTextTexture(&textureId, temp.c_str(), font, 330, 190, 0, 0);
	if (viewContent.dxHandle) SokuLib::textureMgr.remove(viewContent.dxHandle);
	viewContent.setTexture2(textureId, 0, 0, 330, 190);

	if (package->downloading) {
		this->optionCount = 0;
		temp = "Downloading ...";
	} else if (package->fileExists) {
		temp = (package->isEnabled() ? "Disable<br>" : "Enable<br>");
		this->options[0] = OPTION_ENABLE_DISABLE;
		temp += "Show Files<br>";
		this->options[1] = OPTION_SHOW;
		if (package->requireUpdate) {
			temp += "Update";
			this->options[2] = OPTION_DOWNLOAD;
			this->optionCount = 3;
		} else {
			this->optionCount = 2;
		}
	} else {
		temp = "Download";
		this->options[0] = OPTION_DOWNLOAD;
		this->optionCount = 1;
	}
	SokuLib::textureMgr.createTextTexture(&textureId, temp.c_str(), font, 140, 120, 0, 0);
	if (viewOption.dxHandle) SokuLib::textureMgr.remove(viewOption.dxHandle);
	viewOption.setTexture2(textureId, 0, 0, 140, 120);

	if (viewPreview.dxHandle) SokuLib::textureMgr.remove(viewPreview.dxHandle);
	if (!package->previewName.empty()) {
		int width, height;
		SokuLib::textureMgr.loadTexture(&textureId, package->previewName.c_str(), &width, &height);
		viewPreview.setTexture2(textureId, 0, 0, width, height);
	} else {
		package->downloadPreview();
		viewPreview.dxHandle = 0;
	}
}

void ModMenu::swap(int i, int j) {
	if (i == j) return;
	if (i > j) std::swap(i, j);

	std::list<ShadyCore::Package*> enabled;
	{ std::unique_lock lock(ModPackage::descMutex);
		std::swap(ModPackage::descPackage[i], ModPackage::descPackage[j]);

		for (; i <= j; ++i) if (ModPackage::descPackage[i]->isEnabled())
			enabled.push_back(ModPackage::descPackage[i]->package);
	} if (enabled.size() > 1) ModPackage::basePackage->reorder(enabled.begin(), enabled.end());

	settingsDirty = listDirty = viewDirty = true;
}