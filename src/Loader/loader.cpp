#pragma comment (lib, "shlwapi.lib")
#include <shlwapi.h>
#include <zip.h>
#include <stack>

#define TEXT_SECTION_OFFSET	0x00401000
#define TEXT_SECTION_SIZE	0x00456000

static char basePath[1024 + MAX_PATH];
static char dataPackages[2048];
static int dataPackageCount;
static char zipPackages[2048];
static int zipPackageCount;

extern const BYTE TARGET_HASH[16];
__declspec(selectany) const BYTE TARGET_HASH[16] =
{
	// ver1.10
	// 0x26, 0x8a, 0xfd, 0x82, 0x76, 0x90, 0x3e, 0x16, 0x71, 0x6c, 0x14, 0x29, 0xc6, 0x95, 0x9c, 0x9d
	// ver1.10a
	0xdf, 0x35, 0xd1, 0xfb, 0xc7, 0xb5, 0x83, 0x31, 0x7a, 0xda, 0xbe, 0x8c, 0xd9, 0xf5, 0x3b, 0x2e
};

__inline DWORD TamperCall(DWORD addr, DWORD target) {
	DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
	*reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
	return old;
}

void LoadDataPackages(LPCSTR fileName) {
	reinterpret_cast<void(*)(const char *)>(0x0040D1D0u)(fileName);

	char * file = dataPackages;
	for (int i = 0; i < dataPackageCount; ++i) {
		size_t len = strlen(file);
		if (len) {
			if (::PathIsRelative(file)) {
				::PathAppend(basePath, file);
				reinterpret_cast<void(*)(const char *)>(0x0040D1D0u)(basePath);
				::PathRemoveFileSpec(basePath);
			} else reinterpret_cast<void(*)(const char *)>(0x0040D1D0u)(file);
		} file += len + 1;
	}
}

zip_t* appendZipPackage(zip_t* target, const char* path) {
	zip_t* package = zip_open(path, 0, 0);
	if (!package) return 0;

	zip_int64_t fileCount = zip_get_num_entries(package, 0);
	for (int i = 0; i < fileCount; ++i) {
		zip_source_t* source = zip_source_zip(target, package, i, 0, 0, 0);
		if (!source) continue;

		if (zip_file_add(target, zip_get_name(package, i, 0), source, ZIP_FL_OVERWRITE) == -1)
			zip_source_free(source);
	}

	return package;
}

void LoadZipPackages() {
	int err = 0;
	zip_t* target = zip_open("th123e.dat", ZIP_CREATE, &err);
	if (!target) return;
	std::stack<zip_t*> children;

	char * file = zipPackages;
	for (int i = 0; i < zipPackageCount; ++i) {
		size_t len = strlen(file);
		if (len) {
			zip_t* child;
			if (::PathIsRelative(file)) {
				::PathAppend(basePath, file);
				child = appendZipPackage(target, basePath);
				::PathRemoveFileSpec(basePath);
			} else {
				child = appendZipPackage(target, file);
			}
			if (child) children.push(child);
		} file += len + 1;
	}

	zip_close(target);
	while(children.size()) {
		zip_close(children.top());
		children.pop();
	}
}

void LoadSettings(LPCSTR profilePath) {
	::GetPrivateProfileString("Loader", "DataPackages", "", dataPackages, sizeof dataPackages, profilePath);
	dataPackageCount = 1;
	for (char* i = dataPackages; *i != 0; ++i) if (*i == ',') {
		++dataPackageCount;
		*i = 0;
	}

	::GetPrivateProfileString("Loader", "ZipPackages", "", zipPackages, sizeof zipPackages, profilePath);
	zipPackageCount = 1;
	for (char* i = zipPackages; *i != 0; ++i) if (*i == ',') {
		++zipPackageCount;
		*i = 0;
	}
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return ::memcmp(TARGET_HASH, hash, sizeof TARGET_HASH) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	::GetModuleFileName(hMyModule, basePath, 1024);
	::PathRemoveFileSpec(basePath);
	::PathAppend(basePath, "shady-loader.ini");
	LoadSettings(basePath);
	::PathRemoveFileSpec(basePath);

	DWORD dwOldProtect;
	::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
	TamperCall(0x007FB85F, reinterpret_cast<DWORD>(LoadDataPackages));
	::VirtualProtect(reinterpret_cast<LPVOID>(TEXT_SECTION_OFFSET), TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

	//LoadZipPackages();

	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
