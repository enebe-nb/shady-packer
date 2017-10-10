#include "../Core/package.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "command.hpp"
#include <boost/filesystem.hpp>
#include <iostream>

ShadyCli::Command* ShadyCli::Command::parseArguments(int argc, char* argv[]) {
	if (strcmp(argv[1], "convert") == 0) {
		return new ConvertCommand(argc - 1, &argv[1]);
	} else if (strcmp(argv[1], "extract") == 0) {
		return new ExtractCommand(argc - 1, &argv[1]);
	} else if (strcmp(argv[1], "pack") == 0) {
		return new PackCommand(argc - 1, &argv[1]);
	}
	return 0;
}

void ShadyCli::ConvertCommand::processFile(boost::filesystem::path filename, boost::filesystem::path path) {
	std::ifstream input(filename.native(), std::fstream::binary);
	const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.generic_string().c_str(), input);
	if (type == ShadyCore::FileType::TYPE_UNKNOWN || type == ShadyCore::FileType::TYPE_PACKAGE) {
		if (copyUnknown) {
			boost::filesystem::create_directories(path);
			boost::filesystem::copy_file(filename, path / filename.filename());
		}
	} else {
		filename = path / filename.filename().replace_extension(type.inverseExt);
		boost::filesystem::create_directories(path);
		std::ofstream output(filename.native(), std::fstream::binary);
		ShadyCore::convertResource(type, input, output);
	}
}

ShadyCli::ConvertCommand::ConvertCommand(int argc, char* argv[]) {
	boost::program_options::options_description options = boost::program_options::options_description("Convert Command");
	options.add_options()
		("copy-unknown", "Copy files that can't be converted")
		("output", boost::program_options::value<std::string>()->default_value("."), "Target directory to save")
		("files", boost::program_options::value< std::vector<std::string> >(), "List of files to convert");

    boost::program_options::positional_options_description positional;
    positional.add("files", -1);

	boost::program_options::variables_map map;
	boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
        .options(options)
        .positional(positional)
        .run(), map);

	copyUnknown = map.count("copy-unknown");
	output = map["output"].as<std::string>();
	files = map["files"].as< std::vector<std::string> >();
}

void ShadyCli::ConvertCommand::run() {
	boost::filesystem::path outputPath(output);
	size_t size = files.size();
	for (size_t i = 0; i < size; ++i) {
		boost::filesystem::path path(files[i]);
		if (boost::filesystem::is_directory(path)) {
            for (boost::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!boost::filesystem::is_directory(iter->path())) {
                    processFile(iter->path(), outputPath / boost::filesystem::relative(iter->path().parent_path(), path));
                }
            }
		} else if (boost::filesystem::exists(path)) {
			processFile(path, boost::filesystem::path(output));
		}
	}
}

void ShadyCli::ExtractCommand::processFile(boost::filesystem::path filename) {
	ShadyCore::Package package;
    package.appendPackage(filename.string().c_str());
	package.save(target.c_str(), ShadyCore::Package::DIR_MODE, 0);
}

ShadyCli::ExtractCommand::ExtractCommand(int argc, char* argv[]) {
	boost::program_options::options_description options = boost::program_options::options_description("Extract Command");
	options.add_options()
		("package", boost::program_options::value<std::string>(), "Source package")
		("output", boost::program_options::value<std::string>(), "Target output directory");

    boost::program_options::positional_options_description positional;
    positional.add("package", 1).add("output", 1);

	boost::program_options::variables_map map;
	boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
		.options(options)
		.positional(positional)
		.run(), map);

	source = map["package"].as<std::string>();
	target = map["output"].as<std::string>();
}

void ShadyCli::ExtractCommand::run() {
	boost::filesystem::path sourcePath(source);
	if (boost::filesystem::exists(sourcePath) && !boost::filesystem::is_directory(sourcePath)) {
		processFile(sourcePath);
	} else {
		printf("Source package not found.");
	}
}

void ShadyCli::PackCommand::processFile(boost::filesystem::path filename) {
	package.appendPackage(filename.string().c_str());
}

ShadyCli::PackCommand::PackCommand(int argc, char* argv[]) {
	boost::program_options::options_description options = boost::program_options::options_description("Pack Command");
	options.add_options()
		("mode", boost::program_options::value<std::string>(), "Save mode. One of 'zip', 'data' or 'dir'")
		("output", boost::program_options::value<std::string>(), "Target output file")
		("files", boost::program_options::value< std::vector<std::string> >(), "Source directories or data packages");

	boost::program_options::positional_options_description positional;
	positional.add("mode", 1).add("output", 1).add("files", -1);

	boost::program_options::variables_map map;
	boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
		.options(options)
		.positional(positional)
		.run(), map);

	const std::string& s = map["mode"].as<std::string>();
	if (s == "data") {
		mode = ShadyCore::Package::DATA_MODE;
	} else if (s == "zip") {
		mode = ShadyCore::Package::ZIP_MODE;
	} else if(s == "dir") {
		mode = ShadyCore::Package::DIR_MODE;
	} else {
		throw boost::program_options::validation_error(boost::program_options::validation_error::invalid_option_value);
	}

	output = map["output"].as<std::string>();
	files = map["files"].as< std::vector<std::string> >();
}

void ShadyCli::PackCommand::run() {
	size_t size = files.size();
	for (size_t i = 0; i < size; ++i) {
		boost::filesystem::path path(files[i]);
		if (boost::filesystem::exists(path)) {
			processFile(path);
		}
	}

	package.save(output.c_str(), mode, 0);
}

int main(int argc, char* argv[]) {
	ShadyCli::Command* command = ShadyCli::Command::parseArguments(argc, argv);
	command->run();
	delete command;

	return 0;
}
