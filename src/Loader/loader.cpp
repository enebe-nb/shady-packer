#include <shlwapi.h>
#include "../Core/package.hpp"
#include <SokuLib.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/filesystem.hpp>

#define TEXT_SECTION_OFFSET	0x00401000
#define TEXT_SECTION_SIZE	0x00456000

namespace {
	std::ofstream outlog;
	char basePath[1024];
	ShadyCore::Package package;
	std::vector<std::string> packageList;
	std::vector<std::string> optionList;
	//EventID FileLoaderID;
	ItemID LoadMenuID;

	//HANDLE hMutex;
	typedef void (WINAPI* orig_dealloc_t)(int addr);
	orig_dealloc_t orig_dealloc = (orig_dealloc_t)0x81f6fa;
	typedef int (WINAPI* read_constructor_t)(const char *filename, void *a, void *b);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;

	struct file_entry_t {
		ShadyCore::BasePackageEntry* ptr;
		std::istream* stream;
		int size;
		int bytes_read;
	};
}

__inline DWORD TamperCall(DWORD addr, DWORD target) {
	DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
	*reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
	return old;
}

// -------- SokuEngine Start --------
static int WINAPI reader_stream_destruct(int a) {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value) {
		file_entry_t *file = (file_entry_t *)(ecx_value+4);
		delete file->stream;

		if (a & 1) {
			orig_dealloc(ecx_value);
		}
	}
	return ecx_value;
}

static int WINAPI reader_entry_destruct(int a) {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value) {
		file_entry_t *file = (file_entry_t *)(ecx_value+4);
		file->ptr->close();

		if (a & 1) {
			orig_dealloc(ecx_value);
		}
	}
	return ecx_value;
}

static int WINAPI reader_read(char *dest, int size) {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return 0;

	file_entry_t *file = (file_entry_t *)(ecx_value + 4);
	file->bytes_read = 0;
	if ((int)file->stream->tellg() + size > file->size) return 0;
	file->stream->read(dest, size);
	file->bytes_read = size;

	return 1;
}

static void WINAPI reader_seek(int offset, int whence) {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return;

	file_entry_t *file = (file_entry_t *)(ecx_value + 4);
	switch(whence) {
	case 0:
		if (offset > file->size) file->stream->seekg(0, std::ios::end);
		else file->stream->seekg(offset, std::ios::beg);
		break;
	case 1:
		if ((int)file->stream->tellg() + offset > file->size) {
			file->stream->seekg(0, std::ios::end);
		} else if ((int)file->stream->tellg() + offset < 0) {
			file->stream->seekg(0, std::ios::beg);
		} else file->stream->seekg(offset, std::ios::cur);
		break;
	case 2:
		if (offset > file->size) file->stream->seekg(0, std::ios::beg);
		else file->stream->seekg(-offset, std::ios::end);
		break;
	}
}

static int WINAPI reader_getSize() {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return 0;

	file_entry_t *file = (file_entry_t *)(ecx_value + 4);
	return file->size;
}

static int WINAPI reader_getRead() {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return 0;

	file_entry_t *file = (file_entry_t *)(ecx_value + 4);
	return file->bytes_read;
}

struct reader_t {
	// ecx == this for all
	int (WINAPI* destruct)(int a);
	int (WINAPI* read)(char *dest, int size);
	int (WINAPI* getRead)();
	void (WINAPI* seek)(int offset, int whence);
	int (WINAPI* getSize)();
};

static reader_t reader_stream = {
	reader_stream_destruct,
	reader_read,
	reader_getRead,
	reader_seek,
	reader_getSize
};

static reader_t reader_entry = {
	reader_entry_destruct,
	reader_read,
	reader_getRead,
	reader_seek,
	reader_getSize
};

static int WINAPI read_constructor(const char *filename, void *a, void *b) {
	int esi_value;
	__asm mov esi_value, esi
	
	size_t len = strlen(filename);
	char* filenameB = new char[len + 1];
	char* extension = 0;
	for (int i = 0; i < len; ++i) {
		if (filename[i] == '/') filenameB[i] = '_';
		else filenameB[i] = tolower(filename[i]);
		if (filenameB[i] == '.') extension = &filenameB[i];
	} filenameB[len] = 0;

	//WaitForSingleObject(hMutex, INFINITE);
	ShadyCore::Package::iterator file = package.findFile(filenameB);

	if (file == package.end() && extension) {
		ShadyCore::FileType type = ShadyCore::FileType::getSimple(filenameB);
		if (type.type != ShadyCore::FileType::TYPE_UNKNOWN) {
			strcpy(extension, type.inverseExt);
			file = package.findFile(filenameB);
		}
	}

	int result;
	if (file != package.end()) {
		file_entry_t *filedata = (file_entry_t *)(esi_value+4);
		ShadyCore::FileType type = ShadyCore::FileType::get(*file);

		if (type.isEncrypted || type.type == ShadyCore::FileType::TYPE_UNKNOWN) {
			filedata->ptr = &*file;
			filedata->stream = &file->open();
			filedata->stream->seekg(0, std::ios::end);
			filedata->size = filedata->stream->tellg();
			filedata->stream->seekg(0, std::ios::beg);
			filedata->bytes_read = 0;
			*(int*)esi_value = (int)&reader_entry;
			
			result = (int)filedata->ptr;
		} else {
			std::istream& input = file->open();
			std::stringstream* buffer = new std::stringstream();
			ShadyCore::convertResource(type, input, *buffer);
			file->close();

			filedata->ptr = &*file;
			filedata->stream = buffer;
			filedata->size = buffer->tellp();
			filedata->bytes_read = 0;
			*(int*)esi_value = (int)&reader_stream;
			
			result = (int)filedata->stream;
		}
	} else {
		__asm mov esi, esi_value
		result = orig_read_constructor(filename, a, b);
	}

	delete[] filenameB;
	//ReleaseMutex(hMutex);
	return result;
}

namespace {
	char inputBuf[1024];
}

void LoadPackage(const char* filename) {
	boost::filesystem::path fname(filename);
	if (fname.is_relative()) fname = basePath / fname;
	if (boost::filesystem::exists(fname)) package.appendPackage(fname.string().c_str());
}

void LoadMenuCallback() {
	if (ImGui::Button("Refresh", ImVec2(80, 20))) {
		optionList = packageList;
		for (boost::filesystem::directory_iterator iter(basePath); iter != boost::filesystem::directory_iterator(); ++iter) {
			if (boost::filesystem::is_directory(iter->path()) ||
				boost::filesystem::is_regular_file(iter->path()) && (iter->path().extension() == ".dat" || iter->path().extension() == ".zip")) {
				
				std::string filename = iter->path().filename().string();
				if (std::find(optionList.begin(), optionList.end(), filename) == optionList.end()) {
					optionList.push_back(filename);
				}
			}
		}
	}
	ImGui::SameLine(0, 10);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 4));
	ImGui::Text("Load from Path:");
	ImGui::SameLine(0, 10);
	if (ImGui::InputText("##hidden", inputBuf, 1024, ImGuiInputTextFlags_EnterReturnsTrue , 0, 0) && boost::filesystem::exists(inputBuf)) {
		LoadPackage(inputBuf);
		packageList.push_back(inputBuf);
		optionList.push_back(inputBuf);
	}

	for (auto& pack : optionList) {
		ImGui::Dummy(ImVec2(6, 20));
		ImGui::SameLine();

		auto iter = std::find(packageList.begin(), packageList.end(), pack);
		bool isActive = iter != packageList.end();
		if (ImGui::Checkbox(pack.c_str(), &isActive)) {
			if (isActive) {
				LoadPackage(pack.c_str());
				packageList.push_back(pack);
				ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION, 0, 0);
				ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE, 0, 0);
				ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_LOWERCASE, 0, 0);
			} else {
				if (iter != packageList.end()) packageList.erase(iter);
				package.clear();
				for (auto& reload : packageList) {
					LoadPackage(reload.c_str());
				}
				ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION, 0, 0);
				ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE, 0, 0);
				ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_LOWERCASE, 0, 0);
			}
		}
	}
	ImGui::PopStyleVar();
}

void LoadSettings() {
	boost::filesystem::path profilePath(basePath);
	profilePath /= "shady-loader.cfg";

	std::ifstream config(profilePath.native());
	std::string line;
	while(std::getline(config, line)) if (line.size() && line[0] != '#') {
		LoadPackage(line.c_str());
		packageList.push_back(line);
		optionList.push_back(line);
	}

	optionList.erase(unique(optionList.begin(), optionList.end()), optionList.end());
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION, 0, 0);
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE, 0, 0);
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_LOWERCASE, 0, 0);
}

extern const BYTE TARGET_HASH[16];
const BYTE TARGET_HASH[16] = { 0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e };

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	outlog.open("output.log");
	::GetModuleFileNameA(hMyModule, basePath, sizeof basePath);
	::PathRemoveFileSpecA(basePath);
	LoadSettings();

	//FileLoaderID = Soku::SubscribeEvent(SokuEvent::FileLoader, (void(*)(void*))FileLoaderCallback);
	LoadMenuID = Soku::AddItem(SokuComponent::EngineMenu, "Package Load", (void(*)(void)) LoadMenuCallback);

	//hMutex = CreateMutex(0, FALSE, 0);
	DWORD dwOldProtect;
	::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
	orig_read_constructor = (read_constructor_t) TamperCall(0x0040D227, reinterpret_cast<DWORD>(read_constructor));
	::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	outlog.close();
	//Soku::UnsubscribeEvent(FileLoaderID);
	Soku::RemoveItem(LoadMenuID);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
