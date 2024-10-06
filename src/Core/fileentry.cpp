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

//-------------------------------------------------------------

ShadyCore::FilePackageEntry::~FilePackageEntry() { if (deleteOnDestroy) std::filesystem::remove(filename); }

std::istream& ShadyCore::FilePackageEntry::open() {
	++openCount;
#ifdef _MSC_VER
	return *new std::ifstream(parent->getBasePath() / filename, std::ios::binary, _SH_DENYWR);
#else
	return *new std::ifstream(parent->getBasePath() / filename, std::ios::binary);
#endif
}

void ShadyCore::FilePackageEntry::close(std::istream& stream) {
	delete &stream;
	if (openCount > 0) --openCount;
	if (disposable) delete this; // TODO && openCount?
}

//-------------------------------------------------------------

void ShadyCore::Package::loadDir(const std::filesystem::path& path) {
	for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
		if (std::filesystem::is_regular_file(iter->path())) {
			std::filesystem::path relPath = std::filesystem::relative(iter->path(), path);
			this->insert(
				ws2sjis(relPath),
				new FilePackageEntry(this, relPath)
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
		FT(FT::TYPE_SCHEMA, FT::SCHEMA_XML, FT::getExtValue(".xml")),
		FT(FT::TYPE_TEXTURE, FT::TEXTURE_DDS, FT::getExtValue(".dds")),
	};
}

ShadyCore::FileType ShadyCore::GetFilePackageDefaultType(const FT& inputType, ShadyCore::BasePackageEntry* entry) {
	return outputTypes[inputType.type];
}

static inline std::string_view removeExt(const std::string_view &s)
{
	size_t pos = s.find_last_of('.');

	if (pos == std::string_view::npos)
		return s;
	return s.substr(0, pos);
}

void ShadyCore::Package::saveDir(const std::filesystem::path& directory, bool recreateStructure) {
	std::shared_lock lock(*this);
	if (!std::filesystem::exists(directory)) std::filesystem::create_directories(directory);
	else if (!std::filesystem::is_directory(directory)) return;

	std::filesystem::path target = std::filesystem::absolute(directory);
	for (auto i = begin(); i != end(); ++i) {
		FileType inputType = i.fileType();
		FileType targetType = GetFilePackageDefaultType(inputType, i->second);

		std::filesystem::path tempFile = ShadyUtil::TempFile();
		std::ofstream output(tempFile, std::ios::binary);
		std::istream& input = i.open();

		ShadyCore::convertResource(targetType.type, inputType.format, input, targetType.format, output);

		i.close(input);
		output.close();

		// Use auto because on Windows sjis2ws returns a std::wstring and on Unix it returns a std::string
		auto filename = recreateStructure ? sjis2ws(removeExt(i->first.actualName)) : sjis2ws(i->first.name);
		std::filesystem::path final = target / targetType.appendExtValue(filename);

		std::filesystem::create_directories(final.parent_path());
		try {
			std::filesystem::rename(tempFile, final);
		} catch (std::filesystem::__cxx11::filesystem_error &) {
			try {
				std::filesystem::remove(final);
			} catch (std::filesystem::__cxx11::filesystem_error &) {}
			std::filesystem::copy(tempFile, final);
			std::filesystem::remove(tempFile);
		}
	}
}

//-------------------------------------------------------------

ShadyCore::Package::iterator ShadyCore::PackageEx::insert(const std::filesystem::path& filename) {
	if (filename.is_relative())
		return Package::insert(ws2sjis(filename), new FilePackageEntry(this, filename));
	else {
		std::filesystem::path relPath = std::filesystem::proximate(filename, std::filesystem::absolute(basePath));
		return Package::insert(ws2sjis(relPath), new FilePackageEntry(this, filename));
	}
}

ShadyCore::Package::iterator ShadyCore::PackageEx::insert(const std::string_view& name, const std::filesystem::path& filename) {
	return Package::insert(name, new FilePackageEntry(this, filename));
}

ShadyCore::Package::iterator ShadyCore::PackageEx::insert(const std::string_view& name, std::istream& data) {
	std::filesystem::path tempFile = ShadyUtil::TempFile();
	std::ofstream output(tempFile, std::ios::binary);

	output << data.rdbuf();

	return Package::insert(name, new FilePackageEntry(this, tempFile, true));
}
