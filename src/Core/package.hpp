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
			const std::string actualName;
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
		// --- DATA ---
		MapType entries;
		const std::filesystem::path basePath;

		static inline struct default_constructor_t { explicit default_constructor_t() = default; } default_constructor;
		inline Package(default_constructor_t, const std::filesystem::path& basePath) : basePath(basePath) {}

		void loadData(const std::filesystem::path&);
		void loadDir(const std::filesystem::path&);
		void loadZip(const std::filesystem::path&);
		void saveData(const std::filesystem::path&);
		void saveDir(const std::filesystem::path&, bool recreateStructure);
		void saveZip(const std::filesystem::path&);

	public:
		class iterator : public MapType::iterator {
		public:
			inline iterator() : MapType::iterator() {}
			inline iterator(const MapType::iterator& i) : MapType::iterator(i) {}
			inline const std::string_view& name() const { return operator*().first.name; }
			inline BasePackageEntry& entry() const { return *operator*().second; }
			FileType fileType() const;
			inline std::istream& open() const { return operator*().second->open(); }
			inline void close(std::istream& s) const { return operator*().second->close(s); }
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

		iterator insert(const std::string_view& name, BasePackageEntry* entry);
		//iterator rename(iterator i, const std::string_view& name);
		iterator alias(const std::string_view& name, BasePackageEntry& entry);

		enum Mode { DATA_MODE, ZIP_MODE, DIR_MODE, FULL_DIR_MODE };
		void save(const std::filesystem::path&, Mode);
		static void underlineToSlash(std::string&);
	};

	class PackageEx : public Package {
	protected:
		std::vector<Package*> groups;

	public:
		inline PackageEx(const std::filesystem::path& basePath = std::filesystem::current_path()) : Package(default_constructor, basePath) {}
		virtual ~PackageEx();

		Package* merge(Package* package);
		Package* demerge(Package* package);
		void erase(Package* package);
		inline Package* merge(const std::filesystem::path& filename)
			{ return merge(new Package(filename.is_relative() ? basePath / filename : filename)); }

		using Package::insert;
		iterator insert(const std::filesystem::path& filename);
		iterator insert(const std::string_view& name, const std::filesystem::path& filename);
		iterator insert(const std::string_view& name, std::istream& data); // uses temporary file

		//iterator erase(iterator i);                   // TODO requires complexes locks
		bool erase(const std::string_view& name);
		void clear();

		template<class Iterator>
		void reorder(const Iterator begin, const Iterator end) {
			for (Iterator i = begin; i != end; ++i) demerge(*i);
			for (Iterator i = begin; i != end; ++i) merge(*i);
		}
	};
}
