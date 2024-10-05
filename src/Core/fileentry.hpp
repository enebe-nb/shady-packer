#pragma once
#include "baseentry.hpp"
#include "package.hpp"

#include <fstream>
#include <deque>

namespace ShadyCore {
	class FilePackageEntry : public BasePackageEntry {
	private:
		std::filesystem::path filename;
		bool deleteOnDestroy;

	public:
		inline FilePackageEntry(Package* parent, const std::filesystem::path& filename, bool deleteOnDestroy = false)
			: BasePackageEntry(parent, std::filesystem::file_size(parent->getBasePath() / filename)), filename(filename), deleteOnDestroy(deleteOnDestroy) {}
		virtual ~FilePackageEntry();

		inline StorageType getStorage() const override final { return TYPE_FILE; }
		//inline bool isOpen() const override final { return fileStream.size(); }
		std::istream& open() override final;
		void close(std::istream&) override final;
	};

	ShadyCore::FileType GetFilePackageDefaultType(const ShadyCore::FileType& inputType, ShadyCore::BasePackageEntry* entry);
}