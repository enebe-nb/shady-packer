#pragma once
#include "baseentry.hpp"
#include <fstream>

namespace ShadyCore {
	class FilePackageEntry : public BasePackageEntry {
	private:
		const char* filename;
		std::ifstream fileStream;
		bool deleteOnDestroy;
	public:
		FilePackageEntry(const char* name, const char* filename, bool deleteOnDestroy = false);
		virtual ~FilePackageEntry();

		inline EntryType getType() const override final { return TYPE_FILE; }
		inline bool isOpen() const override final { return fileStream.is_open(); }
		inline std::istream& open() override final { fileStream.open(filename, std::ios::binary); return fileStream; }
		inline void close() override final { fileStream.close(); }
	};
}