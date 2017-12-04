#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"

void ShadyCli::MergeCommand::buildOptions(cxxopts::Options& options) {
	options.add_options()
		("p,palette", "For each PNG file specified in `files`, it makes the image indexed by this palette.", cxxopts::value<std::string>())
		("x,pattern", "For each XML file specified in `files`, it merges data into this pattern.", cxxopts::value<std::string>())
		("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

static inline ShadyCore::Palette* getPalette(std::string filename) {
    if (!boost::filesystem::is_regular_file(filename)) {
        printf("File \"%s\" is not readable.\n", filename.c_str());
        return 0;
    }

    std::ifstream input(filename.c_str(), std::ios::binary);
    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.c_str(), input);
    if (type != ShadyCore::FileType::TYPE_PALETTE) {
        printf("File \"%s\" is not a palette.\n", filename.c_str());
        return 0;
    }

    return (ShadyCore::Palette*)ShadyCore::readResource(type, input);
}

static inline ShadyCore::Pattern* getPattern(std::string filename) {
    if (!boost::filesystem::is_regular_file(filename)) {
        printf("File \"%s\" is not readable.\n", filename.c_str());
        return 0;
    }

    std::ifstream input(filename.c_str(), std::ios::binary);
    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.c_str(), input);
    if (type != ShadyCore::FileType::TYPE_PATTERN) {
        printf("File \"%s\" is not a pattern.\n", filename.c_str());
        return 0;
    }

    return (ShadyCore::Pattern*)ShadyCore::readResource(type, input);
}

static inline void savePattern(ShadyCore::Pattern* pattern, std::string filename) {
    std::fstream file(filename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.c_str(), file);

    file.seekp(0);
    ShadyCore::writeResource(pattern, file, type.isEncrypted);
}

void ShadyCli::MergeCommand::run(const cxxopts::Options& options) {
    auto& files = options["files"].as<std::vector<std::string> >();
    ShadyCore::Palette* palette = 0;
    ShadyCore::Pattern* pattern = 0;

    if (options.count("palette")) {
        palette = getPalette(options["palette"].as<std::string>());
    }

    if (options.count("pattern")) {
        pattern = getPattern(options["pattern"].as<std::string>());
    }

	size_t size = files.size();
	for (size_t i = 0; i < size; ++i) {
		boost::filesystem::path path(files[i]);
		if (boost::filesystem::is_directory(path)) {
            for (boost::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!boost::filesystem::is_directory(iter->path())) {
                    if (palette) processPalette(palette, iter->path());
                    if (pattern) processPattern(pattern, iter->path());
                }
            }
		} else if (boost::filesystem::exists(path)) {
			if (palette) processPalette(palette, path);
            if (pattern) processPattern(pattern, path);
		}
	}

    if (palette) delete palette;
    if (pattern) {
        savePattern(pattern, options["pattern"].as<std::string>());
        delete pattern;
    }
}