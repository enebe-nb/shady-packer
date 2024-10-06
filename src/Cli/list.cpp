//
// Created by PinkySmile on 05/10/24.
//

#include "command.hpp"
#include "../Core/package.hpp"

void ShadyCli::ListCommand::buildOptions() {
	options.add_options()
		("s,sort", "How to sort the output: `unsorted`, `type` or `path`.", cxxopts::value<std::string>()->default_value("unsorted"))
		("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
		;

	options.parse_positional("files");
	options.positional_help("<files>...");
}

std::string typeToString(ShadyCore::FileType type)
{
	switch (type.type) {
	case ShadyCore::FileType::TYPE_TEXT:
		return "[TEXT]   ";
	case ShadyCore::FileType::TYPE_TABLE:
		return "[TABLE]  ";
	case ShadyCore::FileType::TYPE_LABEL:
		return "[LABEL]  ";
	case ShadyCore::FileType::TYPE_IMAGE:
		return "[IMAGE]  ";
	case ShadyCore::FileType::TYPE_PALETTE:
		return "[PALETTE]";
	case ShadyCore::FileType::TYPE_SFX:
		return "[SFX]    ";
	case ShadyCore::FileType::TYPE_BGM:
		return "[BGM]    ";
	case ShadyCore::FileType::TYPE_SCHEMA:
		return "[SCHEMA] ";
	case ShadyCore::FileType::TYPE_TEXTURE:
		return "[TEXTURE]";
	default:
		return "[UNKNOWN]";
	}
}

bool ShadyCli::ListCommand::run(const cxxopts::ParseResult& options) {
	ShadyCore::PackageEx package;
	std::string sort = options["sort"].as<std::string>();
	auto &files = options["files"].as<std::vector<std::string>>();

	size_t i = files.size();

	while (i--) {
		std::filesystem::path path(files[i]);

		if (std::filesystem::exists(path)) {
			package.merge(path);
		}
	}

	std::vector<std::pair<std::string, std::string>> names;

	names.reserve(package.size());
	for (auto &files : package)
		names.emplace_back(typeToString(files.first.fileType), files.first.actualName);

	if (sort == "type") {
		std::sort(names.begin(), names.end(), [](auto &a, auto &b) -> bool {
			return a.first < b.first;
		});
	} else if (sort == "path") {
		std::sort(names.begin(), names.end(), [](auto &a, auto &b) -> bool {
			return a.second < b.second;
		});
	} else if (sort != "unsorted") {
		std::cout << "Invalid sort." << std::endl;
		return false;
	}
	for (auto &name : names)
		std::cout << name.first << ": " << name.second << std::endl;
	package.clear();
	return true;
}