#include "package.hpp"
#include "resource/readerwriter.hpp"
#include "util/tempfiles.hpp"

#include <fstream>
#include <filesystem>

ShadyCore::Package::~Package() {
	for (auto entry : entries) {
		delete entry.second;
	}
}

bool ShadyCore::Package::addOrReplace(BasePackageEntry* entry) {
    auto iterator = entries.find(entry->getName());
    if (iterator == entries.end()) {
		std::lock_guard<Package> lock(*this);
		entries[entry->getName()] = entry;
		return true;
    } else {
        delete iterator->second; // TODO Implement late delete (maybe?)
        iterator->second = entry;
        return false;
    }
}

int ShadyCore::Package::appendPackage(const char* filename) {
    if (std::filesystem::is_directory(filename)) {
        return appendDirPackage(filename);
    }

    if (!std::filesystem::is_regular_file(filename)) throw; // TODO better error handling
	char magicWord[6];

    std::ifstream input(filename, std::ios::binary);
	input.unsetf(std::ios::skipws);
	input.read(magicWord, 6);
	input.seekg(0);

	int id;
	if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
		id = appendZipPackage(input, filename);
	} else {
		id = appendDataPackage(input, filename);
	}

    input.close();
	return id;
}

ShadyCore::Package::iterator ShadyCore::Package::detachFile(iterator iter) {
	std::lock_guard<Package> lock(*this);
	delete &*iter;
	return entries.erase(iter.iter);
}

ShadyCore::Package::iterator ShadyCore::Package::renameFile(iterator iter, const char* name) {
	addOrReplace(new RenamedPackageEntry(&*iter, allocateString(name)));
	std::lock_guard<Package> lock(*this);
	return entries.erase(iter.iter);
}

void ShadyCore::Package::save(const char* filename, Mode mode, Callback* callback, void* userData) {
	if (mode == DIR_MODE) return saveDirectory(filename, callback, userData);

	std::filesystem::path target = std::filesystem::absolute(filename);
	if (!std::filesystem::exists(target.parent_path()))
		std::filesystem::create_directories(target.parent_path());

    std::filesystem::path tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile.string(), std::ios::binary);
	if (mode == DATA_MODE) saveData(output, callback, userData);
    if (mode == ZIP_MODE) saveZip(output, callback, userData);
	output.close();

    std::filesystem::copy_file(tempFile, target, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(tempFile);
}

void ShadyCore::Package::clear() {
	std::lock_guard<Package> lock(*this);
	for (auto entry : entries) {
		delete entry.second;
	} entries.clear();
	StrAllocator::clear();
}
