#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"
#include <fstream>

void ShadyCli::MergeCommand::buildOptions() {
    options.add_options()
        ("p,palette", "For each PNG file specified in `files`, it makes the image indexed by this palette.", cxxopts::value<std::string>())
        ("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

static inline ShadyCore::Palette* getPalette(std::string filename) {
    if (!std::filesystem::is_regular_file(filename)) {
        printf("File \"%s\" is not readable.\n", filename.c_str());
        return 0;
    }

    std::ifstream input(filename, std::ios::binary);
    ShadyCore::FileType type = ShadyCore::FileType::get(filename.c_str());
    if (type != ShadyCore::FileType::TYPE_PALETTE) {
        printf("File \"%s\" is not a palette.\n", filename.c_str());
        return 0;
    }

    auto palette = ShadyCore::createResource(ShadyCore::FileType::TYPE_PALETTE);
    ShadyCore::getResourceReader(type)(palette, input);
    return (ShadyCore::Palette*)palette;
}

bool ShadyCli::MergeCommand::run(const cxxopts::ParseResult& result) {
    auto& files = result["files"].as<std::vector<std::string> >();
    ShadyCore::Palette* palette = 0;

    if (result.count("palette")) {
        palette = getPalette(result["palette"].as<std::string>());
    }

    size_t size = files.size();
    for (size_t i = 0; i < size; ++i) {
        std::filesystem::path path(files[i]);
        if (std::filesystem::is_directory(path)) {
            for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!std::filesystem::is_directory(iter->path())) {
                    if (palette) processPalette(palette, iter->path());
                }
            }
        } else if (std::filesystem::exists(path)) {
            if (palette) processPalette(palette, path);
        }
    }

    if (palette) ShadyCore::destroyResource(ShadyCore::FileType::TYPE_PALETTE, palette);
    return true;
}