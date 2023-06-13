#pragma once
#include "baseentry.hpp"
#include "package.hpp"

#include <fstream>
#include <deque>

namespace ShadyCore {
	class FilePackageEntry : public BasePackageEntry {
	private:
		std::filesystem::path filename;
		std::deque<std::ifstream> fileStream;
		bool deleteOnDestroy;
	public:
		inline FilePackageEntry(Package* parent, const std::filesystem::path& filename, bool deleteOnDestroy = false)
			: BasePackageEntry(parent, std::filesystem::file_size(parent->getBasePath() / filename)), filename(filename), deleteOnDestroy(deleteOnDestroy) {}
		virtual ~FilePackageEntry();

		inline StorageType getStorage() const override final { return TYPE_FILE; }
		inline bool isOpen() const override final { return fileStream.size(); }
#ifdef _MSC_VER
		inline std::istream& open() override final { fileStream.emplace_back(parent->getBasePath() / filename, std::ios::binary, _SH_DENYWR); return fileStream.back(); }
#else
		inline std::istream& open() override final { fileStream.emplace_back(parent->getBasePath() / filename, std::ios::binary); return fileStream.back(); }
#endif
		inline void close() override final { fileStream.front().close(); fileStream.pop_front(); if (disposable && !fileStream.size()) delete this; }
	};

	ShadyCore::FileType GetFilePackageDefaultType(const ShadyCore::FileType& inputType, ShadyCore::BasePackageEntry* entry);
}