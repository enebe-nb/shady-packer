#pragma once
#include <istream>

namespace ShadyCore {
	class Package;
	class BasePackageEntry {
	protected:
		Package* const parent;
		const unsigned int size;
	public:
		inline BasePackageEntry(Package* parent, unsigned int size = 0) : parent(parent), size(size) {}
		virtual ~BasePackageEntry() {}
		enum StorageType { TYPE_DATA, TYPE_ZIP, TYPE_FILE, TYPE_STREAM };

		inline Package* getParent() const { return parent; }
		inline const unsigned int& getSize() const { return size; }

		virtual StorageType getStorage() const = 0;
		virtual bool isOpen() const = 0;
		virtual std::istream& open() = 0;
		virtual void close() = 0;
	};

}
