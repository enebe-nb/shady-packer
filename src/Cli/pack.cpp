#include "command.hpp"
#include "../Core/package.hpp"

static inline ShadyCore::Package::Mode getMode(std::string mode) {
    if (mode == "data") return ShadyCore::Package::DATA_MODE;
    if (mode == "dir") return ShadyCore::Package::DIR_MODE;
    if (mode == "zip") return ShadyCore::Package::ZIP_MODE;
    return (ShadyCore::Package::Mode)-1;
}

static inline ShadyCore::PackageFilter::Filter getFilter(std::string filter) {
    if (filter == "from-zip-text") return ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION;
    if (filter == "to-zip-text") return ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION;
    if (filter == "to-zip") return ShadyCore::PackageFilter::FILTER_TO_ZIP_CONVERTER;
    if (filter == "decrypt-all") return ShadyCore::PackageFilter::FILTER_DECRYPT_ALL;
    if (filter == "encrypt-all") return ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL;
    if (filter == "to-underline") return ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE;
    if (filter == "to-slash") return ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH;
    if (filter == "default") return (ShadyCore::PackageFilter::Filter)-2;
    return (ShadyCore::PackageFilter::Filter)-1;
}

static void outputCallback(void*, const char * name, unsigned int index, unsigned int size) {
    printf("\r\x1B[K%03d/%03d -> %40s", index, size, name);
    fflush(stdout);
}

static inline ShadyCore::PackageFilter::Filter filterDefault(ShadyCore::Package& package, ShadyCore::Package::Mode mode) {
    ShadyCore::PackageFilter::Filter filters = ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION;
    switch (mode) {
    case ShadyCore::Package::DATA_MODE:
        filters |= ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL;
        filters |= ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH;
        break;
    case ShadyCore::Package::DIR_MODE:
        filters |= ShadyCore::PackageFilter::FILTER_DECRYPT_ALL;
        break;
    case ShadyCore::Package::ZIP_MODE:
        filters |= ShadyCore::PackageFilter::FILTER_TO_ZIP_CONVERTER;
        filters |= ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION;
        filters |= ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE;
        break;
    }
    return filters;
}

void ShadyCli::PackCommand::buildOptions() {
    options.add_options()
        ("o,output", "Target output file or directory.", cxxopts::value<std::string>())
        ("m,mode", "Output type to save: `data`, `dir` or `zip`. Saving to a directory will extract all files.", cxxopts::value<std::string>()->default_value("dir"))
        ("f,filter", "apply selected filter before saving. `from-zip-text`, `to-zip-text`, `to-zip`, `decrypt-all`, `encrypt-all`, `to-underline`, `to-slash` or `default` to apply all common filters for the current mode.", cxxopts::value<std::vector<std::string> >())
        ("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

bool ShadyCli::PackCommand::run(const cxxopts::ParseResult& options) {
    ShadyCore::Package package;
    ShadyCore::Package::Mode mode = getMode(options["mode"].as<std::string>());
    std::string output = options["output"].as<std::string>();
    auto& filters = options["filter"].as<std::vector<std::string> >();
    auto& files = options["files"].as<std::vector<std::string> >();

    if (output.empty()) {
        printf("No output.\n");
        return false;
    }

    if (mode == -1) {
        printf("Unknown mode \"%s\".\n", options["mode"].as<std::string>().c_str());
        return false;
    }

    size_t size = files.size();
    for (size_t i = 0; i < size; ++i) {
        std::filesystem::path path(files[i]);
        if (std::filesystem::exists(path)) {
            package.appendPackage(path.string().c_str());
        }
    }

    if (package.empty()) {
        printf("Nothing to save.\n");
        return false;
    }

    size = filters.size();
    for (size_t i = 0; i < size; ++i) {
        ShadyCore::PackageFilter::Filter filter = getFilter(filters[i]);
        if (filter == -1) {
            printf("\r\x1B[KSkipping unknown filter \"%s\".\n", filters[i].c_str());
        } else if (filter == -2) {
            printf("\r\x1B[KApplying default filters.\n");
            ShadyCore::PackageFilter::apply(package, filterDefault(package, mode), -1, outputCallback);
        } else {
            printf("\r\x1B[KApplying filter \"%s\".\n", filters[i].c_str());
            ShadyCore::PackageFilter::apply(package, filter, -1, outputCallback);
        }
    }

    printf("\r\x1B[KSaving to disk.\n");
    package.save(output.c_str(), mode, outputCallback, 0);
    printf("\n");

    return true;
}