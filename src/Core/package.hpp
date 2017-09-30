#pragma once
#include "util/strallocator.hpp"
#include "baseentry.hpp"
#include "resource/readerwriter.hpp"

#include <map>
#include <mutex>
#include <functional>

namespace ShadyCore {
    class StreamPackageEntry : public BasePackageEntry {
    private:
        std::istream& stream;
    public:
        inline StreamPackageEntry(std::istream& stream, const char* name, unsigned int size) : BasePackageEntry(name, size), stream(stream) {};

        inline EntryType getType() const final { return TYPE_STREAM; }
		inline std::istream& open() final { return stream; }
		inline bool isOpen() const final { return true; }
		inline void close() final {}
    };

	class RenamedPackageEntry : public BasePackageEntry {
	private:
		BasePackageEntry* entry;
	public:
		inline RenamedPackageEntry(BasePackageEntry* entry, const char* name) : BasePackageEntry(name, entry->getSize()), entry(entry) {}
		inline ~RenamedPackageEntry() { delete entry; }

		inline EntryType getType() const final { return entry->getType(); }
		inline std::istream& open() final { return entry->open(); }
		inline bool isOpen() const final { return entry->isOpen(); }
		inline void close() final { entry->close(); }
	};

	class Package : public ShadyUtil::StrAllocator, public std::mutex {
    private:
        typedef void (Callback)(const char *, unsigned int, unsigned int);
        struct entryCompare { inline bool operator() (const char* left, const char* right) const { return strcmp(left, right) < 0; } };
		typedef std::map<const char *, BasePackageEntry*, entryCompare> MapType;
        MapType entries;

        bool addOrReplace(BasePackageEntry*);

        void appendDataPackage(std::istream&, const char*);
        void appendDirPackage(const char*);
        void appendZipPackage(std::istream&, const char*);
		void saveData(std::ostream&, Callback*);
		void saveDirectory(const char*, Callback*);
        void saveZip(std::ostream&, Callback*);
	public:
		enum Mode { DATA_MODE, ZIP_MODE, DIR_MODE };

        class iterator {
        private:
            MapType::iterator iter;
        public:
			friend Package;
			inline iterator() {}
			inline iterator(const iterator& iter) : iter(iter.iter) {}
            inline iterator(MapType::iterator iter) : iter(iter) {}
			inline iterator& operator=(const iterator& other) { iter = other.iter; return *this; }
            inline iterator& operator++() { ++iter; return *this; }
			inline iterator operator++(int) { return iterator(iter++); }
			inline iterator& operator--() { --iter; return *this; }
			inline iterator operator--(int) { return iterator(iter--); }
			inline bool operator==(const iterator& other) const { return iter == other.iter; }
			inline bool operator!=(const iterator& other) const { return iter != other.iter; }
            inline BasePackageEntry& operator*() const { return *iter->second; }
			inline BasePackageEntry* operator->() const { return iter->second; }
        };

		virtual ~Package();
		inline iterator begin() { return iterator(entries.begin()); }
        inline iterator end() { return iterator(entries.end()); }
		inline iterator findFile(const char* name) { return iterator(entries.find(name)); }
		
		void appendPackage(const char*);
		void appendFile(const char*, const char *, bool = false);
		void appendFile(const char*, std::istream&);
		
		iterator detachFile(iterator iter);
		inline void detachFile(const char* name) { auto iter = entries.find(name); if (iter != entries.end()) detachFile(iter); }
		iterator renameFile(iterator iter, const char* name);
		inline void renameFile(const char* oldName, const char* newName) { auto iter = entries.find(oldName); if (iter != entries.end()) renameFile(iter, newName); }
		
		void save(const char*, Mode, Callback*);
		inline unsigned int size() { return entries.size(); }
		inline bool empty() { return entries.empty(); }
		void clear();
	};

	class PackageFilter {
	private:
		typedef void (Callback)(const char *, unsigned int, unsigned int);
		Package& package;
		Package::iterator iter;
		inline PackageFilter(Package& package) : package(package) {}
	public:
		enum Filter {
			FILTER_FROM_ZIP_TEXT_EXTENSION,
			FILTER_TO_ZIP_TEXT_EXTENSION,
			FILTER_TO_ZIP_CONVERTER,
			FILTER_DECRYPT_ALL,
			FILTER_ENCRYPT_ALL,
			FILTER_SLASH_TO_UNDERLINE,
			FILTER_UNDERLINE_TO_SLASH,
		};

		static void apply(Package&, Filter, Callback*);
		bool renameEntry(const char*);
		bool convertEntry(const FileType&);
		bool convertData(const FileType&);
	};
}
