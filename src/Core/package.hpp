#pragma once
#include "baseentry.hpp"
#include "resource/readerwriter.hpp"

#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <filesystem>

namespace ShadyCore {
	class PackageEx;
	class Package : public std::shared_mutex {
		friend PackageEx;
	protected:
		class Key {
		public:
			const std::string_view name;
			const FileType fileType;

			Key(const Key&) = delete;
			Key(Key&&) = delete;
			Key(const std::string_view&);
			Key(const std::string_view&, FileType::Type type);
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
			inline BasePackageEntry& entry() const { return *operator*().second; }
			FileType fileType() const;
			inline std::istream& open() { return operator*().second->open(); }
			inline void close() { return operator*().second->close(); }
		};

		Package(const std::filesystem::path& basePath);
		virtual ~Package();

		inline iterator begin() { return entries.begin(); }
		inline iterator end() { return entries.end(); }
		inline iterator find(const std::string_view& name) { return entries.find(name); }
		inline iterator find(const std::string_view& name, FileType::Type type)
			{ Key k(name, type); return entries.find(k); }
		inline size_t size() const { return entries.size(); }
		inline bool empty() const { return entries.empty(); }
		inline const std::filesystem::path& getBasePath() { return basePath; }

		//iterator rename(iterator i, const std::string_view& name);
		iterator alias(const std::string_view& name, BasePackageEntry& entry);

		void save(const std::filesystem::path&, Mode, Callback, void*);
		static void underlineToSlash(std::string&);
	};

	class PackageEx : public Package {
	protected:
		std::vector<Package*> groups;

	public:
		inline PackageEx(const std::filesystem::path& basePath = std::filesystem::current_path()) { this->basePath = basePath; }
		virtual ~PackageEx();

		Package* merge(Package* package);
		Package* demerge(Package* package);
		void erase(Package* package);
		inline Package* merge(const std::filesystem::path& filename)
			{ return merge(new Package(filename.is_relative() ? basePath / filename : filename)); }

		iterator insert(const std::filesystem::path& filename);
		iterator insert(const std::string_view& name, const std::filesystem::path& filename);
		iterator insert(const std::string_view& name, std::istream& data); // uses temporary file

		//iterator erase(iterator i);                   // TODO requires complexes locks
		//iterator erase(const std::string_view& name); // TODO requires complexes locks
		void clear();

		template<class Iterator>
		void reorder(const Iterator begin, const Iterator end) {
			for (Iterator i = begin, i != end; ++i) demerge(*i);
			for (Iterator i = begin, i != end; ++i) merge(*i);
		}
	};
}
