#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"

void ShadyCli::ConvertCommand::buildOptions(cxxopts::Options& options) {
	options.add_options()
		("c,copy-unknown", "Copy files that can't be converted")
		("o,output", "Target directory to save", cxxopts::value<std::string>()->default_value(boost::filesystem::current_path().string()))
		("files", "List of files to convert", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

void ShadyCli::ConvertCommand::processFile(boost::filesystem::path filename, boost::filesystem::path targetPath, bool copyUnknown) {
	std::ifstream input(filename.string(), std::fstream::binary);
	const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.generic_string().c_str(), input);
	if (type == ShadyCore::FileType::TYPE_UNKNOWN || type == ShadyCore::FileType::TYPE_PACKAGE) {
		if (copyUnknown) {
            printf("Copying %s -> %s\n", filename.generic_string().c_str(), (targetPath / filename.filename()).generic_string().c_str());
			boost::filesystem::create_directories(targetPath);
			boost::filesystem::copy_file(filename, targetPath / filename.filename());
		}
	} else {
		boost::filesystem::path targetName = targetPath / filename.filename().replace_extension(type.inverseExt);
        printf("Converting %s -> %s\n", filename.generic_string().c_str(), targetName.generic_string().c_str());
		boost::filesystem::create_directories(targetPath);
		std::ofstream output(targetName.string(), std::fstream::binary);
		ShadyCore::convertResource(type, input, output);
	}
}

void ShadyCli::ConvertCommand::run(const cxxopts::Options& options) {
	boost::filesystem::path outputPath(options["output"].as<std::string>());
    bool copyUnknown = options.count("copy-unknown");
    auto& files = options["files"].as<std::vector<std::string> >();

	size_t size = files.size();
	for (size_t i = 0; i < size; ++i) {
		boost::filesystem::path path(files[i]);
		if (boost::filesystem::is_directory(path)) {
            for (boost::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!boost::filesystem::is_directory(iter->path())) {
                    processFile(iter->path(), outputPath / boost::filesystem::relative(iter->path().parent_path(), path), copyUnknown);
                }
            }
		} else if (boost::filesystem::exists(path)) {
			processFile(path, outputPath, copyUnknown);
		}
	}
}
