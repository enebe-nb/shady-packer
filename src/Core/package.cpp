#include "package.hpp"
#include "fileentry.hpp"
#include "util/tempfiles.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>

static inline std::string_view createNormalizedName(const std::string_view& str) {
	char* buffer = new char[str.size() + 1];
	int dotPos = str.size();
	int i = 0; while(i < str.size()) {
		if (str[i] >= 0x80 && str[i] <= 0xa0 || str[i] >= 0xe0 && str[i] <= 0xff) {
			// sjis character (those stand pictures)
			buffer[i] = str[i];
			++i; buffer[i] = str[i];
		} else if (str[i] == '/' || str[i] == '\\') buffer[i] = '_';
		else buffer[i] = std::tolower(str[i]);
		if (buffer[i] == '.') dotPos = i;
		++i;
	} buffer[i] = '\0';

	return std::string_view(buffer, dotPos);
}

ShadyCore::Package::Key::Key(const std::string_view& str)
	: name(createNormalizedName(str)), fileType(FileType::get(FileType::getExtValue(name.data() + name.size()))) {}

ShadyCore::Package::Key::Key(const std::string_view& str, FileType::Type type)
	: name(createNormalizedName(str)), fileType(type, FileType::FORMAT_UNKNOWN) {}

ShadyCore::Package::~Package() {
	for (auto& entry : entries) {
		if (!entry.second->isOpen()) delete entry.second;
		else entry.second->markAsDisposable();
	}
}

ShadyCore::Package::iterator ShadyCore::Package::insert(const std::string_view& name, BasePackageEntry* entry) {
	std::unique_lock lock(*this);
	auto result = entries.emplace(name, entry);
	if (result.second) return result.first;

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

// class StreamPackageEntry : public BasePackageEntry {
// private:
// 	std::istream& stream;
// public:
// 	inline StreamPackageEntry(int id, std::istream& stream, const char* name, unsigned int size) : BasePackageEntry(id, name, size), stream(stream) {};

// 	inline EntryType getType() const final { return TYPE_STREAM; }
// 	inline std::istream& open() final { return stream; }
// 	inline bool isOpen() const final { return true; }
// 	inline void close() final {}
// };

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
		BasePackageEntry& entry;
	public:
		inline AliasPackageEntry(Package* parent, BasePackageEntry& entry) : BasePackageEntry(parent, entry.getSize()), entry(entry) {}

		inline StorageType getStorage() const final { return entry.getStorage(); }
		inline std::istream& open() final { return entry.open(); }
		inline bool isOpen() const final { return entry.isOpen(); }
		inline void close() final { entry.close(); if (disposable) delete this; }
	};
}

ShadyCore::Package::iterator ShadyCore::Package::alias(const std::string_view& name, BasePackageEntry& entry) {
	BasePackageEntry* newEntry = new AliasPackageEntry(entry.getParent(), entry);
	return insert(name, newEntry);
}

void ShadyCore::Package::save(const std::filesystem::path& filename, Mode mode) {
	if (mode == DIR_MODE) return saveDir(filename);

	std::filesystem::path target = std::filesystem::absolute(filename);
	if (!std::filesystem::exists(target.parent_path()))
		std::filesystem::create_directories(target.parent_path());

	std::filesystem::path tempFile = ShadyUtil::TempFile();
	if (mode == DATA_MODE) saveData(tempFile);
	if (mode == ZIP_MODE) saveZip(tempFile);
	std::filesystem::rename(tempFile, target);
}

ShadyCore::FileType ShadyCore::Package::iterator::fileType() const {
	FileType ft = operator*().first.fileType;
	if (ft.format != FileType::FORMAT_UNKNOWN) return ft;

	switch(ft.type) {
	case FileType::TYPE_TEXT:
		ft.format = ft.extValue == FileType::getExtValue(".cv0")
			&& this->entry().getStorage() != BasePackageEntry::TYPE_ZIP
			? FileType::TEXT_GAME : FileType::TEXT_NORMAL;
		break;
	case FileType::TYPE_TABLE:
		ft.format = ft.extValue == FileType::getExtValue(".cv1")
			&& this->entry().getStorage() != BasePackageEntry::TYPE_ZIP
			? FileType::TABLE_GAME : FileType::TABLE_CSV;
		break;
	case FileType::TYPE_LABEL:
		ft.format = ft.extValue == FileType::getExtValue(".sfl")
			? FileType::LABEL_RIFF : FileType::LABEL_LBL;
		break;
	case FileType::TYPE_IMAGE:
		ft.format = ft.extValue == FileType::getExtValue(".cv2")
			? FileType::IMAGE_GAME : FileType::getExtValue(".png")
			? FileType::IMAGE_PNG : FileType::IMAGE_BMP;
		break;
	case FileType::TYPE_PALETTE:
		ft.format = ft.extValue == FileType::getExtValue(".pal")
			? FileType::PALETTE_PAL : FileType::PALETTE_ACT;
		break;
	case FileType::TYPE_SFX:
		ft.format = ft.extValue == FileType::getExtValue(".cv3")
			? FileType::SFX_GAME : FileType::SFX_GAME;
		break;
	case FileType::TYPE_BGM:
		ft.format = FileType::BGM_OGG;
		break;
	case FileType::TYPE_TEXTURE:
		ft.format = FileType::TEXTURE_DDS;
		break;
	case FileType::TYPE_SCHEMA:
		if (ft.extValue == FileType::getExtValue(".dat")) {
			ft.format = FileType::SCHEMA_GAME_GUI;
		} else if(ft.extValue == FileType::getExtValue(".pat")) {
			auto& name = this->name();
			if (name.size() >= 6 && name.ends_with("effect")
				|| name.size() >= 5 && name.ends_with("stand"))
				ft.format = FileType::SCHEMA_GAME_ANIM;
			else ft.format = FileType::SCHEMA_GAME_PATTERN;
		} else if(ft.extValue == FileType::getExtValue(".xml")) {
			ft.format = FileType::SCHEMA_XML;
		}
		break;
	}

	return ft;
}

ShadyCore::PackageEx::~PackageEx() {
	for (auto& group : groups) {
		delete group;
	}
}

ShadyCore::Package* ShadyCore::PackageEx::merge(Package* child) {
	std::scoped_lock lock(*this, *child);
	groups.push_back(child);
	entries.merge(child->entries);

	return child;
}

ShadyCore::Package* ShadyCore::PackageEx::demerge(Package* child) {
	std::scoped_lock lock(*this, *child);
	//auto iter = std::find(groups.begin(), groups.end(), child);
	auto iter = groups.begin(); while(iter != groups.end() && *iter != child) ++iter;
	if (iter == groups.end()) return 0;

	groups.erase(iter);
	auto i = entries.begin(); while(i != entries.end()) {
		if (child == i->second->getParent()) {
			child->entries.insert(entries.extract(i++));
		} else ++i;
	}

	return child;
}

// ShadyCore::Package::iterator ShadyCore::PackageEx::erase(iterator i) {
// 	std::unique_lock lock(*this);

// 	if (i->second->getParent() == this) {
// 		delete i->second;
// 		return entries.erase(i);
// 	} else {
// 		// TODO fix locks
// 		Package* parent = i->second->getParent();
// 		auto result = parent->entries.insert(entries.extract(i++));
// 		if (!result.inserted) throw std::logic_error("Error reinserting node to parent. This should not happen!"); // TODO test and remove
// 		return i;
// 	}
// }

bool ShadyCore::PackageEx::erase(const std::string_view& name) {
	std::unique_lock lock(*this);
	// lock context is required, don't simplify this function
	iterator i = find(name);

	if (i != end()) {
		if (!i->second->isOpen()) delete i->second;
		else i->second->markAsDisposable();
		entries.erase(i);
		return true;
	}

	return false;
}

void ShadyCore::PackageEx::erase(Package* package) {
	{ std::scoped_lock lock(*this, *package);
	if (!std::erase(groups, package)) return;

	auto i = begin(); while(i != end()) {
		if (package == i.entry().getParent()) {
			if (!i->second->isOpen()) delete i->second;
			else i->second->markAsDisposable();
			i = entries.erase(i);
		} else ++i;
	}}

	delete package;
}

void ShadyCore::PackageEx::clear() {
	std::unique_lock lock(*this);
	for (auto& entry : entries) {
		if (!entry.second->isOpen()) delete entry.second;
		else entry.second->markAsDisposable();
	} entries.clear();
	for (auto group : groups) {
		delete group;
	} groups.clear();
}
