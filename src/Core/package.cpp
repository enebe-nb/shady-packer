#include "package.hpp"
#include "resource/readerwriter.hpp"

#include <fstream>
#include <boost/filesystem.hpp>

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

void ShadyCore::Package::appendPackage(const char* filename) {
    if (boost::filesystem::is_directory(filename)) {
        appendDirPackage(filename);
        return;
    }

    if (!boost::filesystem::is_regular_file(filename)) throw; // TODO better error handling
	char magicWord[6];

    std::ifstream input(filename, std::ios::binary);
	input.unsetf(std::ios::skipws);
	input.read(magicWord, 6);
	input.seekg(0);

	if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
		appendZipPackage(input, filename);
	} else {
		appendDataPackage(input, filename);
	}

    input.close();
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

void ShadyCore::Package::save(const char* filename, Mode mode, Callback* callback) {
	if (mode == DIR_MODE) return saveDirectory(filename, callback);

	boost::filesystem::path target = boost::filesystem::absolute(filename);
	if (!boost::filesystem::exists(target.parent_path()))
		boost::filesystem::create_directories(target.parent_path());

    boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	std::ofstream output(tempFile.native(), std::ios::binary);
	if (mode == DATA_MODE) saveData(output, callback);
    if (mode == ZIP_MODE) saveZip(output, callback);
	output.close();

    boost::filesystem::copy_file(tempFile, target, boost::filesystem::copy_option::overwrite_if_exists);
    boost::filesystem::remove(tempFile);
}

void ShadyCore::Package::clear() {
	std::lock_guard<Package> lock(*this);
	for (auto entry : entries) {
		delete entry.second;
	} entries.clear();
	StrAllocator::clear();
}
