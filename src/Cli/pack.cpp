#include "command.hpp"
#include "../Core/package.hpp"

static inline ShadyCore::Package::Mode getMode(std::string mode) {
    if (mode == "data") return ShadyCore::Package::DATA_MODE;
    if (mode == "dir") return ShadyCore::Package::DIR_MODE;
    if (mode == "zip") return ShadyCore::Package::ZIP_MODE;
    return (ShadyCore::Package::Mode)-1;
}

void ShadyCli::PackCommand::buildOptions() {
    options.add_options()
        ("o,output", "Target output file or directory.", cxxopts::value<std::string>())
        ("m,mode", "Output type to save: `data`, `dir` or `zip`. Saving to a directory will extract all files.", cxxopts::value<std::string>()->default_value("dir"))
        ("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

bool ShadyCli::PackCommand::run(const cxxopts::ParseResult& options) {
    ShadyCore::PackageEx package;
    ShadyCore::Package::Mode mode = getMode(options["mode"].as<std::string>());
    std::string output = options["output"].as<std::string>();
    auto& files = options["files"].as<std::vector<std::string> >();

    if (output.empty()) {
        printf("No output.\n");
        return false;
    }

    if (mode == -1) {
        printf("Unknown mode \"%s\".\n", options["mode"].as<std::string>().c_str());
        return false;
    }

    size_t i = files.size();
    while (i--) {
        std::filesystem::path path(files[i]);
        if (std::filesystem::exists(path)) {
            package.merge(path);
        }
    }

    if (package.empty()) {
        printf("Nothing to save.\n");
        return false;
    }

    printf("\r\x1B[KSaving to disk.\n");
    package.save(output.c_str(), mode);
    printf("\n");

    package.clear();
    return true;
}