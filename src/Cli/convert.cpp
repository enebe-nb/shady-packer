#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"
#include <fstream>

void ShadyCli::ConvertCommand::buildOptions() {
    options.add_options()
        ("c,copy-unknown", "Copy files that can't be converted")
        ("o,output", "Target directory to save", cxxopts::value<std::string>()->default_value(std::filesystem::current_path().string()))
        ("files", "List of files to convert", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

void ShadyCli::ConvertCommand::processFile(std::filesystem::path filename, std::filesystem::path targetPath, bool copyUnknown) {
    std::ifstream input(filename.string(), std::fstream::binary);
    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.generic_string().c_str(), input);
    if (type == ShadyCore::FileType::TYPE_UNKNOWN || type == ShadyCore::FileType::TYPE_PACKAGE) {
        if (copyUnknown) {
            printf("Copying %s -> %s\n", filename.generic_string().c_str(), (targetPath / filename.filename()).generic_string().c_str());
            std::filesystem::create_directories(targetPath);
            std::filesystem::copy_file(filename, targetPath / filename.filename());
        }
    } else {
        std::filesystem::path targetName = targetPath / filename.filename().replace_extension(type.inverseExt);
        printf("Converting %s -> %s\n", filename.generic_string().c_str(), targetName.generic_string().c_str());
        std::filesystem::create_directories(targetPath);
        std::ofstream output(targetName.string(), std::fstream::binary);
        ShadyCore::convertResource(type, input, output);
    }
}

bool ShadyCli::ConvertCommand::run(const cxxopts::ParseResult& result) {
    std::filesystem::path outputPath(result["output"].as<std::string>());
    bool copyUnknown = result.count("copy-unknown");
    auto& files = result["files"].as<std::vector<std::string> >();

    size_t size = files.size();
    for (size_t i = 0; i < size; ++i) {
        std::filesystem::path path(files[i]);
        if (std::filesystem::is_directory(path)) {
            for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!std::filesystem::is_directory(iter->path())) {
                    processFile(iter->path(), outputPath / std::filesystem::relative(iter->path().parent_path(), path), copyUnknown);
                }
            }
        } else if (std::filesystem::exists(path)) {
            processFile(path, outputPath, copyUnknown);
        }
    }

    return true;
}
