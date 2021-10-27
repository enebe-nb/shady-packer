#include "fileentry.hpp"
#include "resource/readerwriter.hpp"
#include "util/tempfiles.hpp"

#ifdef _WIN32
#include <windows.h>
#define CP_SHIFT_JIS 932

inline std::string ws2sjis(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_SHIFT_JIS, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_SHIFT_JIS, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
inline std::wstring sjis2ws(const std::string_view& str) {
    int size_needed = MultiByteToWideChar(CP_SHIFT_JIS, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_SHIFT_JIS, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

#endif

ShadyCore::FilePackageEntry::~FilePackageEntry() { if (deleteOnDestroy) std::filesystem::remove(filename); }

//-------------------------------------------------------------

void ShadyCore::Package::loadDir(const std::filesystem::path& path) {
	for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
		if (std::filesystem::is_regular_file(iter->path())) {
			this->insert(
				ws2sjis(std::filesystem::relative(iter->path(), path)),
				new FilePackageEntry(this, iter->path())
			);
		}
	}
}

namespace {
	using FT = ShadyCore::FileType;
	constexpr FT outputTypes[] = {
		FT(FT::TYPE_UNKNOWN, FT::FORMAT_UNKNOWN),
		FT(FT::TYPE_TEXT, FT::TEXT_NORMAL, FT::getExtValue(".txt")),
		FT(FT::TYPE_TABLE, FT::TABLE_CSV, FT::getExtValue(".csv")),
		FT(FT::TYPE_LABEL, FT::LABEL_LBL, FT::getExtValue(".lbl")),
		FT(FT::TYPE_IMAGE, FT::IMAGE_PNG, FT::getExtValue(".png")),
		FT(FT::TYPE_PALETTE, FT::PALETTE_ACT, FT::getExtValue(".act")),
		FT(FT::TYPE_SFX, FT::SFX_WAV, FT::getExtValue(".wav")),
		FT(FT::TYPE_BGM, FT::BGM_OGG, FT::getExtValue(".ogg")),
		// TODO TYPE_SCHEMA
		FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN, FT::getExtValue(".xml")),
	};
}

void ShadyCore::Package::saveDir(const std::filesystem::path& directory, Callback callback, void* userData) {
	if (!std::filesystem::exists(directory)) std::filesystem::create_directories(directory);
	else if (!std::filesystem::is_directory(directory)) return;

	unsigned int index = 0, fileCount = entries.size();
	std::filesystem::path target(directory);
	target = std::filesystem::absolute(target);
	for (auto i = begin(); i != end(); ++i) {
		auto entry = i->second;
		if (callback) callback(userData, i->first.name.data(), ++index, fileCount);
		FileType inputType = i.fileType();
		FileType targetType = outputTypes[i->first.fileType.type];

		std::filesystem::path tempFile = ShadyUtil::TempFile();
		std::ofstream output(tempFile, std::ios::binary);
		std::istream& input = entry->open();

		if (targetType != FileType::TYPE_SCHEMA) ShadyCore::convertResource(targetType.type, inputType.format, input, targetType.format, output);
		// TODO fix TYPE_SCHEMA
		else switch(inputType.format) {
			case FileType::SCHEMA_XML_ANIM:
			case FileType::SCHEMA_XML_GUI:
			case FileType::SCHEMA_XML_PATTERN:
				ShadyCore::convertResource(inputType.type, inputType.format, input, inputType.format, output); break;

			case FileType::SCHEMA_GAME_ANIM:
				ShadyCore::convertResource(inputType.type, inputType.format, input, FileType::SCHEMA_XML_ANIM, output); break;
			case FileType::SCHEMA_GAME_GUI:
				ShadyCore::convertResource(inputType.type, inputType.format, input, FileType::SCHEMA_XML_GUI, output); break;
			case FileType::SCHEMA_GAME_PATTERN:
				ShadyCore::convertResource(inputType.type, inputType.format, input, FileType::SCHEMA_XML_PATTERN, output); break;
		}

		entry->close();
		output.close();

		std::wstring filename = sjis2ws(i->first.name);
		std::filesystem::rename(tempFile, target / targetType.appendExtValue(filename));
	}
}
