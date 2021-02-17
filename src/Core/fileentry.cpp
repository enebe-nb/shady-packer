#include "fileentry.hpp"
#include "package.hpp"
#include "resource/readerwriter.hpp"
#include <filesystem>

ShadyCore::FilePackageEntry::FilePackageEntry(int id, const char* name, const char* filename, bool deleteOnDestroy)
	: BasePackageEntry(id, name, std::filesystem::file_size(filename)), filename(filename), deleteOnDestroy(deleteOnDestroy) {}

ShadyCore::FilePackageEntry::~FilePackageEntry() {
	if (deleteOnDestroy) std::filesystem::remove(filename);
}

//-------------------------------------------------------------

int ShadyCore::Package::appendDirPackage(const char* basePath) {
	for (std::filesystem::recursive_directory_iterator iter(basePath), end; iter != end; ++iter) {
		if (std::filesystem::is_regular_file(iter->path())) {
			const char* filename = allocateString(iter->path().string().c_str());
			const char* name = allocateString(std::filesystem::relative(filename, basePath).generic_string().c_str());
			addOrReplace(new FilePackageEntry(nextId, name, filename));
		}
	}
	return nextId++;
}

int ShadyCore::Package::appendFile(const char* name, const char* filename, int id) {
	if (std::filesystem::exists(filename) && std::filesystem::is_regular_file(filename)) {
		addOrReplace(new FilePackageEntry(id >= 0 ? id : nextId, allocateString(name), allocateString(filename), id >= 0));
	} else throw; // TODO better error handling
	if (id < 0) id = nextId++;
	return id;
}

int ShadyCore::Package::appendFile(const char* name, std::istream& input) {
	std::filesystem::path tempFile = std::filesystem::temp_directory_path() / std::tmpnam(nullptr);
	std::ofstream output(tempFile.string().c_str(), std::ios::binary);

	char buffer[4096]; int read;
	while (read = input.read(buffer, 4096).gcount()) {
		output.write(buffer, read);
	} output.close();

	addOrReplace(new FilePackageEntry(nextId, allocateString(name), allocateString(tempFile.string().c_str()), true));
	return nextId++;
}

void ShadyCore::Package::saveDirectory(const char* directory, Callback* callback, void* userData) {
	char buffer[4096];
	if (!std::filesystem::exists(directory)) {
		std::filesystem::create_directories(directory);
	}
	else if (!std::filesystem::is_directory(directory)) return;

	unsigned int index = 0, fileCount = entries.size();
	std::filesystem::path target(directory);
	std::filesystem::absolute(target);
	for (auto entry : entries) {
		std::filesystem::path filename(target / entry.first);
		if (!std::filesystem::exists(filename.parent_path()))
			std::filesystem::create_directories(filename.parent_path());

		if (callback) callback(userData, entry.first, ++index, fileCount);
		std::filesystem::path tempFile = std::filesystem::temp_directory_path() / std::tmpnam(nullptr);
		std::ofstream output(tempFile.string(), std::ios::binary);
		std::istream& input = entry.second->open();

		int read = input.read(buffer, 4096).gcount();
		while (read) {
			output.write(buffer, read);
			read = input.read(buffer, 4096).gcount();
		}

		entry.second->close();
		output.close();

		std::filesystem::copy_file(tempFile, filename, std::filesystem::copy_options::overwrite_existing);
		std::filesystem::remove(tempFile);
	}
}
