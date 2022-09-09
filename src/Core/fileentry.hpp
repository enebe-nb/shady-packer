#pragma once
#include "baseentry.hpp"
#include "package.hpp"

#include <fstream>

namespace ShadyCore {
	class FilePackageEntry : public BasePackageEntry {
	private:
		std::filesystem::path filename;
		std::ifstream fileStream;
		bool deleteOnDestroy;
	public:
		inline FilePackageEntry(Package* parent, const std::filesystem::path& filename, bool deleteOnDestroy = false)
			: BasePackageEntry(parent, std::filesystem::file_size(parent->getBasePath() / filename)), filename(filename), deleteOnDestroy(deleteOnDestroy) {}
		virtual ~FilePackageEntry();

		inline StorageType getStorage() const override final { return TYPE_FILE; }
		inline bool isOpen() const override final { return fileStream.is_open(); }
#ifdef _MSC_VER
		inline std::istream& open() override final { fileStream.open(parent->getBasePath() / filename, std::ios::binary, _SH_DENYWR); return fileStream; }
#else
		inline std::istream& open() override final { fileStream.open(parent->getBasePath() / filename, std::ios::binary); return fileStream; }
#endif
		inline void close() override final { fileStream.close(); if (disposable) delete this; }
	};

	ShadyCore::FileType GetFilePackageDefaultType(const ShadyCore::FileType& inputType, ShadyCore::BasePackageEntry* entry);
}