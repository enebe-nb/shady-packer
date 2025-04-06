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

	constexpr int listHeight = 286;
	constexpr int rowHeight = 16;
	constexpr int rowsInList = listHeight / rowHeight;

	struct IntlFn {
		void* (* LoadTranslationPack)(const char* name) = 0;
		void (* FreeTranslationPack)(void* pack) = 0;
		const char* (* GetTranslation)(void* pack, const char* name, const char* defName) = 0;
		bool (* LocalizeFont)(const char* name, SokuLib::FontDescription* fontDesc) = 0;

		IntlFn() {
			auto handle = GetModuleHandleA("th123intl.dll");
			if (handle) {
				LoadTranslationPack = reinterpret_cast<void* (*)(const char*)>
					(GetProcAddress(handle, "LoadTranslationPack"));
				FreeTranslationPack = reinterpret_cast<void (*)(void*)>
					(GetProcAddress(handle, "FreeTranslationPack"));
				GetTranslation = reinterpret_cast<const char* (*)(void*, const char*, const char*)>
					(GetProcAddress(handle, "GetTranslation"));
				LocalizeFont = reinterpret_cast<bool (*)(const char*, SokuLib::FontDescription*)>
					(GetProcAddress(handle, "LocalizeFont"));
			}
		}
	} *intlFn = 0;

	SokuLib::FontDescription fontTitle {
		"Verdana",
		0xff, 0xa0, 0xff, 0xa0, 0xff, 0xff,
		20, 400,
		false, true, false,
		100000, 0, 0, 0, 2
	};

	SokuLib::FontDescription fontDesc {
		"Verdana",
		0xff, 0xa0, 0xff, 0xa0, 0xff, 0xff,
		14, 300,
		false, true, true,
		100000, 0, 0, 0, 2
	};

	struct {
		const char* localPackage = "<color 404040>This is a local Package.</color>";
		const char* version = "Version: <color 606060>{}</color>";
		const char* creator = "Creator: <color 606060>{}</color>";
		const char* description = "Description: <color 606060>{}</color>";
		const char* tagsBegin = "Tags: <color 606060>";
		const char* tagsElement = "#{}";
		const char* tagsSeparator = "  ";
		const char* tagsEnd = "</color>";

		const char* downloading = "Downloading ...";
		const char* optDisable = "Disable";
		const char* optEnable = "Enable";
		const char* optShow = "Show Files";
		const char* optUpdate = "Update";
		const char* optDownload = "Download";
	} tMsg;
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
	// just set values, render is done on CDesign
	if (size <= rowsInList) {
		this->scrollHeight = 0;
	} else {
		this->scrollHeight = size < view ? listHeight : view*listHeight/size;
		this->scrollBar->y1 = listHeight*offset/size + this->scrollHeight - listHeight;
	}

	for (int i = 0; i < view && i < size - offset; ++i) {
		this->renderLine(x, y + i*rowHeight, i + offset);
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
	if (this->loadMessage) this->loadMessage->active = this->names.size() == 0;
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
	if (!intlFn) intlFn = new IntlFn();
	if (intlFn->LoadTranslationPack && intlFn->GetTranslation) {
		translationPack    = intlFn->LoadTranslationPack("shady");

		tMsg.localPackage  = intlFn->GetTranslation(translationPack, "menu-local-package", tMsg.localPackage);
		tMsg.version       = intlFn->GetTranslation(translationPack, "menu-version", tMsg.version);
		tMsg.creator       = intlFn->GetTranslation(translationPack, "menu-creator", tMsg.creator);
		tMsg.description   = intlFn->GetTranslation(translationPack, "menu-description", tMsg.description);
		tMsg.tagsBegin     = intlFn->GetTranslation(translationPack, "menu-tags-begin", tMsg.tagsBegin);
		tMsg.tagsElement   = intlFn->GetTranslation(translationPack, "menu-tags-element", tMsg.tagsElement);
		tMsg.tagsSeparator = intlFn->GetTranslation(translationPack, "menu-tags-separator", tMsg.tagsSeparator);
		tMsg.tagsEnd       = intlFn->GetTranslation(translationPack, "menu-tags-end", tMsg.tagsEnd);

		tMsg.downloading   = intlFn->GetTranslation(translationPack, "menu-downloading", tMsg.downloading);
		tMsg.optDisable    = intlFn->GetTranslation(translationPack, "menu-option-disable", tMsg.optDisable);
		tMsg.optEnable     = intlFn->GetTranslation(translationPack, "menu-option-enable", tMsg.optEnable);
		tMsg.optShow       = intlFn->GetTranslation(translationPack, "menu-option-show", tMsg.optShow);
		tMsg.optUpdate     = intlFn->GetTranslation(translationPack, "menu-option-update", tMsg.optUpdate);
		tMsg.optDownload   = intlFn->GetTranslation(translationPack, "menu-option-download", tMsg.optDownload);
	}

	design.loadResource("shady/downloader.dat");
	ModPackage::LoadFromFilesystem();
	(guide.*SokuLib::union_cast<void (SokuLib::Guide::*)(unsigned int)>(0x443160))(89); // Init
	guide.active = true;

	design.getById((SokuLib::CDesign::Sprite**)&modList.scrollBar, 101);
	modList.scrollBar->active = true;
	modList.scrollBar->gauge.set(&modList.scrollHeight, 0, listHeight);
	modCursor.set(&SokuLib::inputMgrs.input.verticalAxis, modList.names.size(), 0, rowsInList);

	design.getById(&modList.loadMessage, 300);
	ModPackage::LoadFromRemote();
}

ModMenu::~ModMenu() {
	if (translationPack && intlFn->FreeTranslationPack) intlFn->FreeTranslationPack(translationPack);

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
			modCursor.max = modList.names.size() == 0 ? 1 : modList.names.size();
		}
		if (viewDirty && modCursor.pos < modList.names.size()) this->updateView(modCursor.pos);
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
		} else if (modCursor.max > modCursor.dRows) {
			// page rolling
			if (SokuLib::inputMgrs.input.horizontalAxis == 1) {
				modCursor.pgDn();
				if (modCursor.pgPos > modCursor.max - modCursor.dRows)
					modCursor.pgPos = modCursor.max - modCursor.dRows;
				SokuLib::playSEWaveBuffer(0x27);
				viewDirty = true;
			} else if (SokuLib::inputMgrs.input.horizontalAxis == -1) {
				modCursor.pgUp();
				SokuLib::playSEWaveBuffer(0x27);
				viewDirty = true;
			}
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

	if (modCursor.pos >= modCursor.pgPos + rowsInList) {
		modCursor.pgPos = modCursor.pos - rowsInList + 1;
	} else if (modCursor.pos < modCursor.pgPos) {
		modCursor.pgPos = modCursor.pos;
	}

	SokuLib::CDesign::Object* pos;
	design.getById(&pos, 100);
	if (this->state == 0 && modList.getLength()) {
		SokuLib::MenuCursor::render(pos->x2, pos->y2 + (modCursor.pos - modCursor.pgPos)*rowHeight, 256);
		if (orderCursor >= modCursor.pgPos && orderCursor < modCursor.pgPos + rowsInList)
			SokuLib::MenuCursor::render(pos->x2, pos->y2 + (orderCursor - modCursor.pgPos)*rowHeight, 256);
	}
	modList.renderScroll(pos->x2, pos->y2, modCursor.pgPos, modList.getLength(), rowsInList);

	design.getById(&pos, 200);
	viewTitle.render(pos->x2, pos->y2);
	design.getById(&pos, 201);
	viewContent.render(pos->x2, pos->y2);
	design.getById(&pos, 202);
	if (this->state == 1) SokuLib::MenuCursor::render(pos->x2, pos->y2 + viewCursor.pos*rowHeight, 120);
	viewOption.render(pos->x2, pos->y2);
	design.getById(&pos, 203);
	if(viewPreview.dxHandle) viewPreview.renderScreen(pos->x2, pos->y2, pos->x2 + 200, pos->y2 + 150);

	(guide.*SokuLib::union_cast<void (SokuLib::Guide::*)()>(0x443260))(); // Render
	return 0;
}

void ModMenu::updateView(int index) {
	auto cp = th123intl::GetTextCodePage();
	ModPackage* package = ModPackage::descPackage[index];
	SokuLib::SWRFont font; font.create();
	int textureId;

	if (intlFn->LocalizeFont) intlFn->LocalizeFont("shady.menu.title", &fontTitle);
	font.setIndirect(fontTitle);
	{
		std::string name; th123intl::ConvertCodePage(CP_UTF8, package->name, cp, name);
		SokuLib::textureMgr.createTextTexture(&textureId, name.c_str(), font, 330, 24, 0, 0);
	}
	if (viewTitle.dxHandle) SokuLib::textureMgr.remove(viewTitle.dxHandle);
	viewTitle.setTexture2(textureId, 0, 0, 330, 24);

	if (intlFn->LocalizeFont) intlFn->LocalizeFont("shady.menu.description", &fontDesc);
	font.setIndirect(fontDesc);
	std::string temp;
	if (package->isLocal()) temp = tMsg.localPackage;
	else {
		std::string cpStr;
		th123intl::ConvertCodePage(CP_UTF8, package->version(), cp, cpStr);
		temp += std::vformat(tMsg.version, std::make_format_args(cpStr)) + "<br>";
		th123intl::ConvertCodePage(CP_UTF8, package->creator(), cp, cpStr);
		temp += std::vformat(tMsg.creator, std::make_format_args(cpStr)) + "<br>";
		th123intl::ConvertCodePage(CP_UTF8, package->description(), cp, cpStr);
		temp += std::vformat(tMsg.description, std::make_format_args(cpStr)) + "<br>";
		temp += tMsg.tagsBegin;
		for (int i = 0; i < package->tags.size(); ++i) {
			if (i > 0) temp += tMsg.tagsSeparator;
			th123intl::ConvertCodePage(CP_UTF8, package->tags[i], cp, cpStr);
			temp += std::vformat(tMsg.tagsElement, std::make_format_args(cpStr));
		}
		temp += tMsg.tagsEnd;
	}

	SokuLib::textureMgr.createTextTexture(&textureId, temp.c_str(), font, 330, 190, 0, 0);
	if (viewContent.dxHandle) SokuLib::textureMgr.remove(viewContent.dxHandle);
	viewContent.setTexture2(textureId, 0, 0, 330, 190);

	if (package->downloading) {
		this->optionCount = 0;
		temp = tMsg.downloading;
	} else if (package->fileExists) {
		temp = (package->isEnabled() ? tMsg.optDisable : tMsg.optEnable); temp += "<br>";
		this->options[0] = OPTION_ENABLE_DISABLE;
		temp += tMsg.optShow; temp += "<br>";
		this->options[1] = OPTION_SHOW;
		if (package->requireUpdate) {
			temp += tMsg.optUpdate;
			this->options[2] = OPTION_DOWNLOAD;
			this->optionCount = 3;
		} else {
			this->optionCount = 2;
		}
	} else {
		temp = tMsg.optDownload;
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