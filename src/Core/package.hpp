#pragma once
#include "baseentry.hpp"
#include "resource/readerwriter.hpp"

#include <unordered_map>
#include <mutex>
#include <filesystem>

namespace ShadyCore {
	/*
	class StreamPackageEntry : public BasePackageEntry {
	private:
		std::istream& stream;
	public:
		inline StreamPackageEntry(int id, std::istream& stream, const char* name, unsigned int size) : BasePackageEntry(id, name, size), stream(stream) {};

		inline EntryType getType() const final { return TYPE_STREAM; }
		inline std::istream& open() final { return stream; }
		inline bool isOpen() const final { return true; }
		inline void close() final {}
	};
	*/

	class PackageEx;
	class Package : public std::mutex {
		friend PackageEx;
	protected:
		class Key {
		public:
			std::string_view name;
			FileType fileType;

			Key(const Key&) = delete;
			Key(Key&&) = delete;
			Key(const std::string_view&);
			inline ~Key() { delete name.data(); }

			struct hash {
				inline std::size_t operator()(const Key& v) const noexcept {
					return std::hash<std::string_view>()(v.name) ^ std::hash<short>()(v.fileType.type);
				}
			};

			inline bool operator==(const Key& o) const { return name == o.name && fileType.type == o.fileType.type; }
			inline bool operator!=(const Key& o) const { return name != o.name || fileType.type != o.fileType.type; }
		};

		using MapType = std::unordered_map<Key, BasePackageEntry*, Key::hash>;
		using Callback = void (*)(void*, const char*, unsigned int, unsigned int);

		// --- DATA ---
		MapType entries;
		std::filesystem::path basePath;

		Package() = default;
		virtual MapType::iterator insert(const std::string_view& name, BasePackageEntry* entry);

		void loadData(const std::filesystem::path&);
		void loadDir(const std::filesystem::path&);
		void loadZip(const std::filesystem::path&);
		void saveData(const std::filesystem::path&, Callback, void*);
		void saveDir(const std::filesystem::path&, Callback, void*);
		void saveZip(const std::filesystem::path&, Callback, void*);

	public:
		enum Mode { DATA_MODE, ZIP_MODE, DIR_MODE };
		//using iterator = MapType::iterator;
		class iterator : public MapType::iterator {
		public:
			inline iterator() : MapType::iterator() {}
			inline iterator(const MapType::iterator& i) : MapType::iterator(i) {}
			inline const std::string_view& name() const { return operator*().first.name; }
			FileType fileType() const;
			//inline std::istream& open() { return operator*().second->open(); }
			//inline void close() { return operator*().second->close(); }
		};

		inline Package(const std::filesystem::path& basePath);
		static Package* create(const std::filesystem::path&);

		virtual ~Package();
		inline iterator begin() { return entries.begin(); }
		inline iterator end() { return entries.end(); }
		inline iterator find(const std::string_view& name) { return entries.find(name); }
		inline size_t size() const { return entries.size(); }
		inline bool empty() const { return entries.empty(); }
		inline const std::filesystem::path& getBasePath() { return basePath; }

		//iterator rename(iterator i, const std::string_view& name);
		iterator alias(const std::string_view& name, BasePackageEntry* entry);

		void save(const std::filesystem::path&, Mode, Callback, void*);
		//static FileType::Format getFormat(iterator);
		//static inline FileType getType(iterator i) { FileType type = i->first.fileType; type.format = getFormat(i); return type; }
		static void underlineToSlash(std::string&);
	};

	class PackageEx : public Package {
	protected:
		std::vector<Package*> groups;

	public:
		inline PackageEx(const std::filesystem::path& basePath = std::filesystem::current_path()) { this->basePath = basePath; }
		virtual ~PackageEx();

		Package* merge(const std::filesystem::path& filename);

		iterator insert(const std::filesystem::path& filename);
		iterator insert(const std::string_view& name, const std::filesystem::path& filename);
		iterator insert(const std::string_view& name, std::istream& data); // uses temporary file

		iterator erase(iterator i);
		iterator erase(const std::string_view& name);
		void erase(Package* package);

		void clear();
	};
/*
	class PackageFilter {
	private:
		typedef void (Callback)(void*, const char *, unsigned int, unsigned int);
		Package& package;
		Package::iterator iter;
		inline PackageFilter(Package& package) : package(package) {}
	public:
		enum Filter : int {
			FILTER_NONE	= 0,
			// Sequence Order
			FILTER_FROM_ZIP_TEXT_EXTENSION	= 1 << 0,
			FILTER_DECRYPT_ALL				= 1 << 1,
			FILTER_ENCRYPT_ALL				= 1 << 2,
			FILTER_TO_ZIP_CONVERTER			= 1 << 3,
			FILTER_TO_ZIP_TEXT_EXTENSION	= 1 << 4,
			FILTER_SLASH_TO_UNDERLINE		= 1 << 5,
			FILTER_UNDERLINE_TO_SLASH		= 1 << 6,
			FILTER_TO_LOWERCASE				= 1 << 7,
		};

		static void apply(Package&, Filter, int = -1, Callback* = 0, void* = 0);
		bool renameEntry(const char*);
		bool convertEntry(const FileType&);
		bool convertData(const FileType&);
	};
*/
}

//inline ShadyCore::PackageFilter::Filter& operator|=(ShadyCore::PackageFilter::Filter& l, const ShadyCore::PackageFilter::Filter& r)
//	{return l = (ShadyCore::PackageFilter::Filter)(l | r);}
