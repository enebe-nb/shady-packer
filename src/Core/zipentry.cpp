#include "zipentry.hpp"
#include "package.hpp"
#include "util/tempfiles.hpp"

#include <cstring>
#include <zip.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace {
	struct StreamData {
		std::istream zipStream; // must be first
		ShadyCore::ZipStream zipBuffer;
		inline StreamData() : zipStream(&zipBuffer) {}
	};
#ifdef _DEBUG
	std::mutex streamLock;
	std::unordered_map<ShadyCore::ZipPackageEntry*, std::list<StreamData>> streamList;
#endif
}

std::streambuf::int_type ShadyCore::ZipStream::underflow() {
	if (hasBufVal) return bufVal;

	char value;
	int read = zip_fread((zip_file_t*)innerFile, &value, 1);
	if (read > 0) {
		hasBufVal = true;
		return bufVal = traits_type::to_int_type(value);
	} else if (read < 0) throw std::runtime_error("Zip Read Error: underflow");
	return traits_type::eof();
}

std::streamsize ShadyCore::ZipStream::xsgetn(char_type* buffer, std::streamsize size) {
	bool noBufVal = true;
	if (hasBufVal && size > 0) {
		*buffer++ = bufVal;
		--size; ++pos;
		noBufVal = hasBufVal = false;
	}

	size = zip_fread((zip_file_t*)innerFile, buffer, size);
	if (size < 0) throw std::runtime_error("Zip Read Error: xsgetn");
	pos += size;
	return size + (noBufVal ? 0 : 1);
}

std::streambuf::int_type ShadyCore::ZipStream::uflow() {
	if (hasBufVal) {
		++pos;
		hasBufVal = false;
		return bufVal;
	}

	char value;
	int read = zip_fread((zip_file_t*)innerFile, &value, 1);
	if (read > 0) {
		++pos;
		return traits_type::to_int_type(value);
	} else if (read < 0) throw std::runtime_error("Zip Read Error: uflow");
	return traits_type::eof();
}

std::streambuf::pos_type ShadyCore::ZipStream::seekoff(off_type spos, std::ios::seekdir sdir, std::ios::openmode mode) {
	if (mode != std::ios::in) return pos_type(off_type(-1)); // readonly stream;

	if (zip_file_is_seekable((zip_file_t*)innerFile)) {
		if (hasBufVal) {
			++pos; hasBufVal = false;
			if (sdir == std::ios::cur) --spos;
		}

		switch(sdir) {
		case std::ios::cur:
			if (0 == zip_fseek((zip_file_t*)innerFile, spos, SEEK_CUR))
				return pos += spos;
			else break;
		case std::ios::end:
			if (0 == zip_fseek((zip_file_t*)innerFile, spos, SEEK_END))
				return pos = size+spos;
			else break;
		case std::ios::beg:
			if (0 == zip_fseek((zip_file_t*)innerFile, spos, SEEK_SET))
				return pos = spos;
			else break;
		}
	}

	if (sdir == std::ios::cur) {
		if (spos >= 0) {
			while (spos--) uflow();
			return pos;
		} else pos += spos;
	}

	if (sdir == std::ios::end) {
		off_type off = size + spos - pos;
		if (off >= 0) {
			while (off--) uflow();
			return pos;
		} else pos += off;
	}

	if (sdir == std::ios::beg) {
		if (spos >= pos) {
			while (spos > pos) uflow();
			return pos;
		} else pos = spos;
	}

	zip_fclose((zip_file_t*)innerFile);
	innerFile = zip_fopen_index((zip_t*)pkgFile, index, 0);
	char* ignoreBuf = new char[pos];
	auto ret = zip_fread((zip_file_t*)innerFile, ignoreBuf, pos);
	delete[] ignoreBuf;
	if (ret < 0) throw std::runtime_error("Zip Read Error: seekoff");

	return pos;
}

std::streambuf::pos_type ShadyCore::ZipStream::seekpos(pos_type spos, std::ios::openmode mode) {
	return seekoff(spos, std::ios::beg, mode);
}

void ShadyCore::ZipStream::open(const std::filesystem::path& file, const std::string& name) {
#ifdef _WIN32
	auto handle = CreateFileW(file.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE) return;
	pkgFile = zip_open_from_source(zip_source_win32handle_create(handle, 0, 0, 0), ZIP_RDONLY, 0);
#else
	FILE *handle = fopen(file.string().c_str(), "rb");
	if (!handle) return;
	pkgFile = zip_open_from_source(zip_source_filep_create(handle, 0, 0, 0), ZIP_RDONLY, 0);
#endif

	zip_stat_t stat;
	zip_stat_init(&stat);
	zip_stat((zip_t*)pkgFile, name.c_str(), 0, &stat);
	const auto requiredStats = ZIP_STAT_INDEX|ZIP_STAT_SIZE;
	if (stat.valid & requiredStats != requiredStats) throw std::runtime_error("Zip Read Error: invalid stats");
	index = stat.index;
	size = stat.size;
	pos = 0;

	innerFile = zip_fopen_index((zip_t*)pkgFile, index, 0);
	if (!innerFile) throw std::runtime_error("Zip Read Error: open (zip_file)");
}

void ShadyCore::ZipStream::close() {
	zip_fclose((zip_file_t*)innerFile);
	zip_close((zip_t*)pkgFile);
}

//-------------------------------------------------------------

typedef struct {
	ShadyCore::BasePackageEntry* entry;
	std::istream* stream;
} ZipData;

static inline ZipData* createZipReader(ShadyCore::BasePackageEntry* entry) { return new ZipData{entry, 0}; }

static zip_int64_t zipInputFunc(void* userdata, void* data, zip_uint64_t len, zip_source_cmd_t command) {
	ZipData* zipData = (ZipData*)userdata;
	zip_source_args_seek_t* seekdata;
	std::ios::seekdir seekdir;

	switch (command) {
	case ZIP_SOURCE_SUPPORTS:
		return ZIP_SOURCE_SUPPORTS_SEEKABLE;
	case ZIP_SOURCE_OPEN:
		zipData->stream = &zipData->entry->open();
		return zipData->stream->good() ? 0 : -1;
	case ZIP_SOURCE_READ:
		return zipData->stream->read((char*)data, len).gcount();
	case ZIP_SOURCE_CLOSE:
		zipData->entry->close(*zipData->stream);
		zipData->stream = 0;
		return 0;
	case ZIP_SOURCE_STAT:
		zip_stat_init((zip_stat_t*)data);
		((zip_stat_t*)data)->size = zipData->entry->getSize();
		((zip_stat_t*)data)->valid = ZIP_STAT_SIZE;
		return sizeof(zip_stat_t);
	case ZIP_SOURCE_ERROR:
		if (zipData->stream && zipData->stream->fail()) {
			zip_error_t error; zip_error_init(&error);
			zip_error_set(&error, ZIP_ER_INTERNAL, 0);
			return zip_error_to_data(&error, data, len);
		}
		memset(data, 0, 2 * sizeof(int));
		return 2 * sizeof(int);
	case ZIP_SOURCE_FREE:
		delete zipData;
		return 0;

	case ZIP_SOURCE_SEEK:
		seekdata = ZIP_SOURCE_GET_ARGS(zip_source_args_seek, data, len, 0);
		seekdir = seekdata->whence == SEEK_SET ? std::ios::beg : seekdata->whence == SEEK_CUR ? std::ios::cur : std::ios::end;
		return ((std::istream*)zipData->stream)->seekg(seekdata->offset, seekdir).fail();
	case ZIP_SOURCE_TELL:
		return ((std::istream*)zipData->stream)->tellg();
	}

	throw; // Force unsupported error
}

static zip_int64_t zipOutputFunc(void* userdata, void* data, zip_uint64_t len, zip_source_cmd_t command) {
	std::ostream* stream = (std::ostream*)userdata;
	zip_source_args_seek_t* seekdata;
	std::ios::seekdir seekdir;

	switch (command) {
	case ZIP_SOURCE_SUPPORTS:
		return ZIP_SOURCE_SUPPORTS_WRITABLE; // TODO
	case ZIP_SOURCE_OPEN:
		stream->seekp(0);
		return 0;
	case ZIP_SOURCE_CLOSE:
		return 0;
	case ZIP_SOURCE_STAT:
		zip_stat_init((zip_stat_t*)data);
		return sizeof(zip_stat_t);
	case ZIP_SOURCE_ERROR:
		if (stream && stream->fail()) {
			zip_error_t error; zip_error_init(&error);
			zip_error_set(&error, ZIP_ER_INTERNAL, 0);
			return zip_error_to_data(&error, data, len);
		}
		memset(data, 0, 2 * sizeof(int));
		return 2 * sizeof(int);

    case ZIP_SOURCE_FREE: return 0;
	case ZIP_SOURCE_BEGIN_WRITE: return 0;
	case ZIP_SOURCE_COMMIT_WRITE: return 0;

    case ZIP_SOURCE_WRITE:
        stream->write((char*)data, len);
        return len;
	case ZIP_SOURCE_SEEK_WRITE:
		seekdata = ZIP_SOURCE_GET_ARGS(zip_source_args_seek, data, len, 0);
		seekdir = seekdata->whence == SEEK_SET ? std::ios::beg : seekdata->whence == SEEK_CUR ? std::ios::cur : std::ios::end;
		return ((std::ostream*)stream)->seekp(seekdata->offset, seekdir).fail();
	case ZIP_SOURCE_TELL_WRITE:
        return ((std::ostream*)stream)->tellp();
	}

	throw; // Force unsupported error
}

//-------------------------------------------------------------

std::istream& ShadyCore::ZipPackageEntry::open() {
	++openCount;
#ifdef _DEBUG
	auto& streamData = streamList[this].emplace_back();
#else
	auto& streamData = *(new StreamData());
#endif
	streamData.zipBuffer.open(parent->getBasePath(), name);
	return streamData.zipStream;
}

void ShadyCore::ZipPackageEntry::close(std::istream& zipStream) {
#ifdef _DEBUG
	auto& list = streamList[this];
	{ std::lock_guard lock(streamLock);
	for (auto iter = list.begin(); iter != list.end(); ++iter) {
		if (&zipStream == &iter->zipStream) {
			--openCount;
			iter->zipBuffer.close();
			list.erase(iter);
			break;
		}
	} }
#else
	--openCount;
	((StreamData*)&zipStream)->zipBuffer.close();
	delete ((StreamData*)&zipStream);
#endif

	if (disposable && openCount == 0) delete this;
}

//-------------------------------------------------------------

void ShadyCore::Package::loadZip(const std::filesystem::path& path) {
#ifdef _WIN32
	auto handle = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE) return;
	auto file = zip_open_from_source(zip_source_win32handle_create(handle, 0, 0, 0), ZIP_RDONLY, 0);
#else
	FILE *filep = fopen(path.c_str(), "rb");
	if (!filep) return;
	auto file = zip_open_from_source(zip_source_filep_create(filep, 0, 0, 0), ZIP_RDONLY, 0);
#endif

	zip_int64_t count = zip_get_num_entries(file, 0);
	entries.reserve(entries.size() + count);
	for (zip_int64_t i = 0; i < count; ++i) {
		zip_stat_t fileStat;
		zip_stat_index(file, i, 0, &fileStat);

		std::string_view name(fileStat.name);
		if (name.ends_with('/')) continue;
		this->insert(fileStat.name, new ZipPackageEntry(this, fileStat.name, fileStat.size));
	}

	zip_close(file);
}

namespace {
	using FT = ShadyCore::FileType;
	constexpr FT outputTypes[] = {
		FT(FT::TYPE_UNKNOWN, FT::FORMAT_UNKNOWN),
		FT(FT::TYPE_TEXT, FT::TEXT_NORMAL, FT::getExtValue(".cv0")),
		FT(FT::TYPE_TABLE, FT::TABLE_CSV, FT::getExtValue(".cv1")),
		FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl")),
		FT(FT::TYPE_IMAGE, FT::IMAGE_PNG, FT::getExtValue(".png")),
		FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal")),
		FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3")),
		FT(FT::TYPE_BGM, FT::BGM_OGG, FT::getExtValue(".ogg")),
		FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN),
		FT(FT::TYPE_TEXTURE, FT::TEXTURE_DDS, FT::getExtValue(".dds"))
	};
}

ShadyCore::FileType ShadyCore::GetZipPackageDefaultType(const FT& inputType, ShadyCore::BasePackageEntry* entry) {
	if (inputType == FT::TYPE_SCHEMA) {
		FT::Format format = FT::FORMAT_UNKNOWN;
		if (inputType.format == FT::SCHEMA_XML) {
			char buffer[16];
			std::istream& input = entry->open();
			while (input.get(buffer[0]) && input.gcount()) if (buffer[0] == '<') {
				int j = 0;
				for (input.get(buffer[0]); j < 16 && buffer[j] && !strchr(" />", buffer[j]); input.get(buffer[++j]));
				buffer[j] = '\0';
				if (strcmp(buffer, "movepattern") == 0) { format = FT::SCHEMA_GAME_PATTERN; break; }
				if (strcmp(buffer, "animpattern") == 0) { format = FT::SCHEMA_GAME_ANIM; break; }
				if (strcmp(buffer, "layout") == 0) { format = FT::SCHEMA_GAME_GUI; break; }
				if (!strchr("?", buffer[0])) break;
			}
			entry->close(input);
		} else format = inputType.format;
		return FT(FT::TYPE_SCHEMA, format, format == FT::SCHEMA_GAME_GUI ? FT::getExtValue(".dat") : FT::getExtValue(".pat"));
	} return outputTypes[inputType.type];
}

void ShadyCore::Package::saveZip(const std::filesystem::path& filename) {
	std::shared_lock lock(*this);
	std::ofstream output(filename, std::ios::binary);
	zip_source_t* outputSource = zip_source_function_create(zipOutputFunc, &output, 0);
	zip_t* file = zip_open_from_source(outputSource, ZIP_CREATE | ZIP_TRUNCATE | ZIP_CHECKCONS, 0);
	std::vector<std::filesystem::path> convertedFiles;

	for (auto i = begin(); i != end(); ++i) {
		auto entry = i->second;

		zip_source_t* inputSource;
		FileType inputType = i.fileType();
		FileType targetType = GetZipPackageDefaultType(inputType, entry);

		if (inputType.format == targetType.format) {
			inputSource = zip_source_function_create(zipInputFunc, createZipReader(entry), 0);
		} else {
			convertedFiles.push_back(ShadyUtil::TempFile());
			std::ofstream output(convertedFiles.back(), std::ios::binary);
			std::istream& input = entry->open();

			ShadyCore::convertResource(inputType.type, inputType.format, input, targetType.format, output);
			output.close();

			inputSource = zip_source_file_create(convertedFiles.back().string().c_str(), 0, 0, 0);
			entry->close(input);
		}

		std::string filename(i->first.name);
		zip_file_add(file, targetType.appendExtValue(filename).c_str(), inputSource, ZIP_FL_OVERWRITE);
	}

	zip_close(file);
	for (auto& file : convertedFiles) std::filesystem::remove(file);
}

