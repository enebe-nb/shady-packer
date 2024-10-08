//
// Created by PinkySmile on 05/10/24.gi
//

#include "command.hpp"
#include "../Core/package.hpp"
#include "../Core/dataentry.hpp"
#include <fstream>
#include <functional>
#include <set>
#include <map>
#include <zip.h>

#ifdef _WIN32
#include <windows.h>
#endif

void ShadyCli::ListCommand::buildOptions() {
    options.add_options()
        ("s,sort", "How to sort the output: `unsorted`, `type` or `path`.", cxxopts::value<std::string>()->default_value("unsorted"))
        ("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

static std::string typeToString(ShadyCore::FileType type) {
    switch (type.type) {
    case ShadyCore::FileType::TYPE_TEXT:
        return "[TEXT]    ";
    case ShadyCore::FileType::TYPE_TABLE:
        return "[TABLE]   ";
    case ShadyCore::FileType::TYPE_LABEL:
        return "[LABEL]   ";
    case ShadyCore::FileType::TYPE_IMAGE:
        return "[IMAGE]   ";
    case ShadyCore::FileType::TYPE_PALETTE:
        return "[PALETTE] ";
    case ShadyCore::FileType::TYPE_SFX:
        return "[SFX]     ";
    case ShadyCore::FileType::TYPE_BGM:
        return "[BGM]     ";
    case ShadyCore::FileType::TYPE_SCHEMA:
        return "[SCHEMA]  ";
    case ShadyCore::FileType::TYPE_TEXTURE:
        return "[TEXTURE] ";
    default:
        return "[UNKNOWN] ";
    }
}

static void readDataInnerFiles(const std::filesystem::path& path, std::function<void (const std::string&)> push) {
    std::ifstream input(path, std::ios::binary);

    unsigned short fileCount; input.read((char*)&fileCount, 2);
    unsigned int listSize; input.read((char*)&listSize, 4);
    ShadyCore::DataListFilter inputFilter(input.rdbuf(), listSize + 6, listSize);
    std::istream filteredInput(&inputFilter);

    for (int i = 0; i < fileCount; ++i) {
        unsigned char nameSize;
        filteredInput.ignore(8);
        filteredInput.read((char*)&nameSize, 1);
        std::string name; name.resize(nameSize);
        filteredInput.read(name.data(), nameSize);
        push(name);
    }
}

static void readDirInnerFiles(const std::filesystem::path& path, std::function<void (const std::string&)> push) {
	for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
		if (std::filesystem::is_regular_file(iter->path())) {
			auto name = std::filesystem::relative(iter->path(), path).string();
			push(name);
		}
	}
}

static void readZipInnerFiles(const std::filesystem::path& path, std::function<void (const std::string&)> push) {
#ifdef _WIN32
    auto handle = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle == INVALID_HANDLE_VALUE) return;
    auto file = zip_open_from_source(zip_source_win32handle_create(handle, 0, 0, 0), ZIP_RDONLY, 0);
#else
    FILE *filep = fopen(path.c_str(), "rb");
    if (!filep) return;
    auto file = zip_open_from_source(zip_source_filep_create(filep, 0, 0, 0), ZIP_RDONLY, 0);
#endif

    zip_int64_t count = zip_get_num_entries(file, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        zip_stat_t fileStat;
        zip_stat_index(file, i, 0, &fileStat);

        std::string name(fileStat.name);
        if (name.ends_with('/')) continue;
        push(name);
    } zip_close(file);
}

static void readInnerFiles(const std::filesystem::path& path, std::function<void (const std::string&)> push) {
    if (std::filesystem::is_directory(path)) return readDirInnerFiles(path, push);
    if (!std::filesystem::is_regular_file(path)) return;

    char magicWord[6];
    std::ifstream input(path, std::ios::binary);
    input.unsetf(std::ios::skipws);
    input.read(magicWord, 6);
    input.close();

    if (strncmp(magicWord, "PK\x03\x04", 4) == 0 || strncmp(magicWord, "PK\x05\x06", 4) == 0 ) {
        return readZipInnerFiles(path, push);
    } else {
        return readDataInnerFiles(path, push);
    }
}

namespace {
    struct fileTypeCompare {
        bool operator()(const ShadyCore::FileType& a, const ShadyCore::FileType& b) const {
            return ((((int)a.format)<<16) + (int)a.type) < ((((int)b.format)<<16) + (int)b.type);
        }
    };
}

bool ShadyCli::ListCommand::run(const cxxopts::ParseResult& options) {
    std::string sort = options["sort"].as<std::string>();
    auto &files = options["files"].as<std::vector<std::string>>();

    if (sort == "unsorted") {
        for (std::filesystem::path file : files) {
            readInnerFiles(file, [](std::string_view str) { std::cout << str << std::endl; });
        }
    } else if (sort == "type") {
        std::multimap<ShadyCore::FileType, std::string, fileTypeCompare> output;
        for (std::filesystem::path file : files) readInnerFiles(file, [&output](const std::string& str) {
            output.emplace(ShadyCore::FileType::get(str.c_str()), str);
        });

        for (auto& iter : output) {
            std::cout << typeToString(iter.first) << iter.second << std::endl;
        }
    } else if (sort == "path") {
        std::set<std::string> output;
        for (std::filesystem::path file : files) readInnerFiles(file, [&output](const std::string& str) {
            output.emplace(str);
        });

        for (auto& name : output) std::cout << name << std::endl;
    } else {
        std::cout << "Invalid sort." << std::endl;
        return false;
    }

    return true;
}