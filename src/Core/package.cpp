#include "package.hpp"
#include "fileentry.hpp"
#include "util/tempfiles.hpp"
#include <fstream>
#include <iostream>
#include <list>
#include <xhash>

ShadyCore::Package::Key::Key(const std::string_view& str) {
	size_t bufferSize = str.size() >= 4 && str[str.size() - 4] == '.' ? str.size() - 3 : str.size() + 1;
	char* buffer = new char[bufferSize];
	int i = 0; while(i < str.size() && str[i] != '.') {
		if (str[i] >= 0x80 && str[i] <= 0xa0 || str[i] >= 0xe0 && str[i] <= 0xff) {
			// sjis character (those stand pictures)
			buffer[i] = str[i];
			++i; buffer[i] = str[i];
		} else if (str[i] == '/' || str[i] == '\\') buffer[i] = '_';
		else buffer[i] = str[i];
		++i;
	} buffer[i] = '\0';

	name = std::string_view(buffer, i);
	if (i < str.size()) fileType = FileType::get(FileType::getExtValue(&str[i]));
}

ShadyCore::Package::~Package() {
	for (auto& entry : entries) {
		delete entry.second;
	}
}

ShadyCore::Package::MapType::iterator ShadyCore::Package::insert(const std::string_view& name, BasePackageEntry* entry) {
	std::lock_guard lock(*this);
	auto result = entries.emplace(name, entry);
	if (result.second) return result.first;

	// TODO collision
	delete entry;
	return end();
}

ShadyCore::Package::Package(const std::filesystem::path& basePath) : basePath(basePath) {
	if (std::filesystem::is_directory(basePath)) { loadDir(basePath); return; }
	if (!std::filesystem::is_regular_file(basePath))
		throw std::runtime_error("'" + basePath.string() + "' isn't a file or directory.");

	char magicWord[6];
	std::ifstream input(basePath, std::ios::binary);
	input.unsetf(std::ios::skipws);
	input.read(magicWord, 6);
	input.close();

	if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
		loadZip(basePath);
	} else {
		loadData(basePath);
	}
}

// ShadyCore::Package::iterator ShadyCore::Package::rename(iterator i, const std::string& name) {
// 	std::lock_guard lock(*this);
// 	//setDirty();
// 	// TODO normalize name?
// 	BasePackageEntry* entry = i->second;
// 	entries.erase(i);
// 	auto result = entries.try_emplace(name, entry);
// 	if (result.second) {
// 		return result.first;
// 	} else {
// 		// TODO collision
// 		delete entry;
// 		return end();
// 	}
// }

namespace ShadyCore {
	class AliasPackageEntry : public BasePackageEntry {
	protected:
		BasePackageEntry* entry;
	public:
		inline AliasPackageEntry(Package* parent, BasePackageEntry* entry) : BasePackageEntry(parent, entry->getSize()), entry(entry) {}

		inline StorageType getStorage() const final { return entry->getStorage(); }
		inline std::istream& open() final { return entry->open(); }
		inline bool isOpen() const final { return entry->isOpen(); }
		inline void close() final { entry->close(); }
	};
}

ShadyCore::Package::iterator ShadyCore::Package::alias(const std::string_view& name, BasePackageEntry* entry) {
	BasePackageEntry* newEntry = new AliasPackageEntry(this, entry);
	return insert(name, newEntry);
}

void ShadyCore::Package::save(const std::filesystem::path& filename, Mode mode, Callback callback, void* userData) {
	if (mode == DIR_MODE) return saveDir(filename, callback, userData);

	std::filesystem::path target = std::filesystem::absolute(filename);
	if (!std::filesystem::exists(target.parent_path()))
		std::filesystem::create_directories(target.parent_path());

	std::filesystem::path tempFile = ShadyUtil::TempFile();
	if (mode == DATA_MODE) saveData(tempFile, callback, userData);
	if (mode == ZIP_MODE) saveZip(tempFile, callback, userData);
	std::filesystem::rename(tempFile, target);
}

ShadyCore::FileType ShadyCore::Package::iterator::fileType() const {
	FileType ft = operator*().first.fileType;
    if (ft.format != FileType::FORMAT_UNKNOWN) return ft;

    switch(ft.type) {
        case FileType::TYPE_TEXT:
            ft.format = ft.extValue == FileType::getExtValue(".cv0")
				&& operator*().second->getStorage() != BasePackageEntry::TYPE_ZIP
				? FileType::TEXT_GAME : FileType::TEXT_NORMAL;
			break;
        case FileType::TYPE_TABLE:
            ft.format = ft.extValue == FileType::getExtValue(".cv1")
				&& operator*().second->getStorage() != BasePackageEntry::TYPE_ZIP
				? FileType::TABLE_GAME : FileType::TABLE_CSV;
			break;
        case FileType::TYPE_SCHEMA:
            if (ft.extValue == FileType::getExtValue(".dat")) {
                std::istream& input = operator*().second->open();
		        uint32_t version; input.read((char*)&version, 4);
		        operator*().second->close();
                if (version == 4) ft.format = FileType::SCHEMA_GAME_GUI;
            } else if(ft.extValue == FileType::getExtValue(".pat")) {
                std::istream& input = operator*().second->open();
		        uint8_t version; input.read((char*)&version, 1);
		        operator*().second->close();
                if (version != 5) break;
				auto& name = operator*().first.name;
                if (name.size() >= 6 && strcmp(name.data() + name.size() - 6, "effect") == 0
                    || name.size() >= 5 && strcmp(name.data() + name.size() - 5, "stand") == 0)
                    ft.format = FileType::SCHEMA_GAME_ANIM;
                else ft.format = FileType::SCHEMA_GAME_PATTERN;
            } else if(ft.extValue == FileType::getExtValue(".xml")) {
                std::istream& input = operator*().second->open();
                char buffer[128];
                while (input.get(buffer[0]) && input.gcount()) {
                    if (buffer[0] == '<') {
                        int j = 0;
                        for (input.get(buffer[0]); buffer[j] && !strchr(" />", buffer[j]); input.get(buffer[++j]));
                        buffer[j] = '\0';
                        if (strcmp(buffer, "movepattern") == 0) { ft.format = FileType::SCHEMA_XML_PATTERN; break; }
                        if (strcmp(buffer, "animpattern") == 0) { ft.format = FileType::SCHEMA_XML_ANIM; break; }
                        if (strcmp(buffer, "layout") == 0) { ft.format = FileType::SCHEMA_XML_GUI; break; }
                        if (!strchr("?", buffer[0])) break;
                    }
                }
                operator*().second->close();
            }
    }

	return ft;
}

ShadyCore::PackageEx::~PackageEx() {
	for (auto& group : groups) {
		delete group;
	}
}

ShadyCore::Package* ShadyCore::PackageEx::merge(const std::filesystem::path& filename) {
	Package* child = new Package(filename.is_relative() ? basePath / filename : filename);

	std::scoped_lock lock(*this, *child);
	groups.push_back(child);
	entries.merge(child->entries);

	return child;
}

ShadyCore::Package::iterator ShadyCore::PackageEx::insert(const std::filesystem::path& filename) {
	return Package::insert(
		std::filesystem::proximate(filename).string(), // TODO convert to shiftjis?
		new FilePackageEntry(this, filename.is_relative() ? basePath / filename : filename)
	);
}

ShadyCore::Package::iterator ShadyCore::PackageEx::insert(const std::string_view& name, const std::filesystem::path& filename) {
	return Package::insert(name, new FilePackageEntry(this, filename.is_relative() ? (basePath / filename) : filename));
}

ShadyCore::Package::iterator ShadyCore::PackageEx::insert(const std::string_view& name, std::istream& data) {
	std::filesystem::path tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile, std::ios::binary);

	output << data.rdbuf(); // TODO speed test
	// char buffer[4096]; int read;
	// while (read = data.read(buffer, 4096).gcount()) {
	// 	output.write(buffer, read);
	// } output.close();

	return Package::insert(name, new FilePackageEntry(this, tempFile, true));
}

ShadyCore::Package::iterator ShadyCore::PackageEx::erase(iterator i) {
	std::lock_guard lock(*this);

	if (i->second->getParent() == this) {
		delete i->second;
		return entries.erase(i);
	} else {
		Package* parent = i->second->getParent();
		auto result = parent->entries.insert(entries.extract(i++));
		if (!result.inserted) throw std::logic_error("Error reinserting node to parent. This should not happen!"); // TODO test and remove
		return i;
	}
}

ShadyCore::Package::iterator ShadyCore::PackageEx::erase(const std::string_view& name) {
	std::lock_guard lock(*this);
	// lock context is required, don't simplify this function
	iterator i = find(name);

	if (i->second->getParent() == this) {
		delete i->second;
		return entries.erase(i);
	} else {
		Package* parent = i->second->getParent();
		auto result = parent->entries.insert(entries.extract(i++));
		if (!result.inserted) throw std::logic_error("Error reinserting node to parent. This should not happen!"); // TODO test and remove
		return i;
	}
}

void ShadyCore::PackageEx::erase(Package* package) {
	std::lock_guard lock(*this);
	if (!std::erase(groups, package)) return;

	auto i = begin(); while(i != end()) {
		if (i->second->getParent() == package) {
			delete i->second;
			i = entries.erase(i);
		} else ++i;
	}

	delete package;
}

void ShadyCore::PackageEx::clear() {
	std::lock_guard lock(*this);
	for (auto& entry : entries) {
		delete entry.second;
	} entries.clear();
	for (auto group : groups) {
		delete group;
	} groups.clear();
}
