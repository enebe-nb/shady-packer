#pragma once
#include <istream>

namespace ShadyCore {
	class BasePackageEntry {
	protected:
		const char* name;
		unsigned int size;
	public:
		inline BasePackageEntry(const char* name = 0, unsigned int size = 0) : name(name), size(size) {}
		virtual ~BasePackageEntry() {}
		enum EntryType { TYPE_DATA, TYPE_ZIP, TYPE_FILE, TYPE_STREAM };

		inline const char* getName() const { return name; }
		inline const unsigned int& getSize() const { return size; }

		virtual EntryType getType() const = 0;
		virtual bool isOpen() const = 0;
		virtual std::istream& open() = 0;
		virtual void close() = 0;
	};

}
