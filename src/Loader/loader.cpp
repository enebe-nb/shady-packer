#include <shlwapi.h>
#include "../Core/package.hpp"
#include <SokuLib.h>
#include <fstream>
#include <sstream>

#define TEXT_SECTION_OFFSET	0x00401000
#define TEXT_SECTION_SIZE	0x00456000

namespace {
	std::ofstream outlog;
	char basePath[1024];
	ShadyCore::Package package;
	//EventID FileLoaderID;

	HANDLE hMutex;
	typedef void (WINAPI* orig_dealloc_t)(int addr);
	orig_dealloc_t orig_dealloc = (orig_dealloc_t)0x81f6fa;
	typedef int (WINAPI* read_constructor_t)(const char *filename, void *a, void *b);
	read_constructor_t orig_read_constructor = (read_constructor_t)0x41c080;

	union file_t {
		struct {
			char *start;
			char *end;
			char *cur;
			int bytes_read;
		} data;

		struct {
			ShadyCore::BasePackageEntry* ptr;
			std::istream* stream;
			int size;
			int bytes_read;
		} entry;
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
		file_t *file = (file_t *)(ecx_value+4);
		delete file->entry.stream;

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
		file_t *file = (file_t *)(ecx_value+4);
		file->entry.ptr->close();

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

	file_t *file = (file_t *)(ecx_value + 4);
	file->entry.bytes_read = 0;
	if ((int)file->entry.stream->tellg() + size > file->entry.size) return 0;
	file->entry.stream->read(dest, size);
	file->data.bytes_read = size;

	return 1;
}

static void WINAPI reader_seek(int offset, int whence) {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return;

	file_t *file = (file_t *)(ecx_value + 4);
	switch(whence) {
	case 0:
		if (offset > file->entry.size) file->entry.stream->seekg(0, std::ios::end);
		else file->entry.stream->seekg(offset, std::ios::beg);
		break;
	case 1:
		if ((int)file->entry.stream->tellg() + offset > file->entry.size) {
			file->entry.stream->seekg(0, std::ios::end);
		} else if ((int)file->entry.stream->tellg() + offset < 0) {
			file->entry.stream->seekg(0, std::ios::beg);
		} else file->entry.stream->seekg(offset, std::ios::cur);
		break;
	case 2:
		if (offset > file->entry.size) file->entry.stream->seekg(0, std::ios::beg);
		else file->entry.stream->seekg(-offset, std::ios::end);
		break;
	}
}

static int WINAPI reader_getSize() {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return 0;

	file_t *file = (file_t *)(ecx_value + 4);
	return file->entry.size;
}

static int WINAPI reader_getRead() {
	int ecx_value;
	__asm mov ecx_value, ecx

	if (ecx_value == 0) return 0;

	file_t *file = (file_t *)(ecx_value + 4);
	return file->data.bytes_read;
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

	WaitForSingleObject(hMutex, INFINITE);
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
		outlog << "filename: " << filenameB << std::endl;
		outlog.flush();

		file_t *filedata = (file_t *)(esi_value+4);
		ShadyCore::FileType type = ShadyCore::FileType::get(*file);

		if (type.isEncrypted || type.type == ShadyCore::FileType::TYPE_UNKNOWN) {
			filedata->entry.ptr = &*file;
			filedata->entry.stream = &file->open();
			filedata->entry.stream->seekg(0, std::ios::end);
			filedata->entry.size = filedata->entry.stream->tellg();
			filedata->entry.stream->seekg(0, std::ios::beg);
			filedata->entry.bytes_read = 0;
			*(int*)esi_value = (int)&reader_entry;
			
			result = (int)filedata->entry.ptr;
		} else {
			outlog << "THERE" << std::endl;
			std::istream& input = file->open();
			std::stringstream* buffer = new std::stringstream();
			ShadyCore::convertResource(type, input, *buffer);
			file->close();

			filedata->entry.ptr = &*file;
			filedata->entry.stream = buffer;
			filedata->entry.size = buffer->tellp();
			filedata->data.bytes_read = 0;
			*(int*)esi_value = (int)&reader_stream;
			
			result = (int)filedata->entry.stream;
		}
	} else {
		__asm mov esi, esi_value
		result = orig_read_constructor(filename, a, b);
	}

	delete[] filenameB;
	ReleaseMutex(hMutex);
	return result;
}

void LoadPackage(const char* filename) {
	if (::PathIsRelative(filename)) {
		char filePath[1024 + MAX_PATH];
		strcpy(filePath, basePath);
		::PathAppendA(filePath, filename);
		if (::PathFileExists(filePath)) package.appendPackage(filePath);
	} else if (::PathFileExists(filename)) package.appendPackage(filename);
}

void LoadSettings() {
	char profilePath[1024 + MAX_PATH];
	char packages[2048];
	strcpy(profilePath, basePath);
	::PathAppendA(profilePath, "shady-loader.ini");
	::GetPrivateProfileStringA("Loader", "Packages", "", packages, sizeof packages, profilePath);

	char* start = packages, *end;
	while (end = strchr(start, ',')) {
		*end = 0;
		outlog << start << std::endl;
		LoadPackage(start);
		start = end + 1;
	} if (strlen(start)) {
		outlog << start << std::endl;
		LoadPackage(start);
	}

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION, 0, 0);
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE, 0, 0);
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_LOWERCASE, 0, 0);
	outlog.flush();
}

/*
void FileLoaderCallback(SokuData::FileLoaderData* data) {
	outlog << data->filename << std::endl;
	outlog.flush();

	ShadyCore::Package::iterator file = package.findFile(data->filename);

	if (file == package.end()) {
		char* filename = new char[strlen(data->filename) + 1];
		strcpy(filename, data->filename);
		char* ext = strrchr(filename, '.');
		ShadyCore::FileType type = ShadyCore::FileType::getSimple(data->filename);
		if (type.type != ShadyCore::FileType::TYPE_UNKNOWN) {
			strcpy(ext, type.inverseExt);
			file = package.findFile(filename);
		}
		delete[] filename;
	}

	if (file != package.end()) {
		ShadyCore::FileType type = ShadyCore::FileType::get(*file);
		if (type.isEncrypted
			//&& type.type != ShadyCore::FileType::TYPE_IMAGE
			) {

			if (strncmp(data->filename, "data_character_alice_attackaa000", 32) == 0) {
				outlog << "------ HERE ------" << std::endl;
			}

			std::istream& fileStream = file->open();
			// TODO streamed vector copy
			data->data.assign(std::istreambuf_iterator<char>(fileStream), std::istreambuf_iterator<char>());
			*data->size = data->data.size();
			file->close();
		}
	}
}
*/

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

	hMutex = CreateMutex(0, FALSE, 0);
	DWORD dwOldProtect;
	::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
	orig_read_constructor = (read_constructor_t) TamperCall(0x0040D227, reinterpret_cast<DWORD>(read_constructor));
	::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

	return TRUE;
}

extern "C" __declspec(dllexport) void AtExit() {
	outlog.close();
	//Soku::UnsubscribeEvent(FileLoaderID);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
