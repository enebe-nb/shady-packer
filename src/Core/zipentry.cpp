#include "zipentry.hpp"
#include "package.hpp"
#include <zip.h>
#include <filesystem>

std::streambuf::int_type ShadyCore::ZipStream::underflow() {
	if (!pool.empty()) return pool.top();

	char value;
	if (zip_fread((zip_file_t*)innerFile, &value, 1)) {
		pool.push(traits_type::to_int_type(value));
		return pool.top();
	} return traits_type::eof();
}

std::streamsize ShadyCore::ZipStream::xsgetn(char_type* buffer, std::streamsize size) {
	int buffered = 0;
	while (!pool.empty() && size) {
		--size;
		buffer[buffered++] = pool.top();
		pool.pop();
	}
	pos += buffered;

	size = zip_fread((zip_file_t*)innerFile, buffer + buffered, size);
	pos += size;
	return size + buffered;
}

std::streambuf::int_type ShadyCore::ZipStream::uflow() {
	if (!pool.empty()) {
		++pos;
		int_type value = pool.top();
		pool.pop();
		return value;
	}

	char value;
	if (zip_fread((zip_file_t*)innerFile, &value, 1)) {
		++pos;
		return traits_type::to_int_type(value);
	} return traits_type::eof();
}

std::streambuf::pos_type ShadyCore::ZipStream::seekoff(off_type spos, std::ios::seekdir sdir, std::ios::openmode mode) {
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
	zip_fread((zip_file_t*)innerFile, ignoreBuf, pos);
	delete[] ignoreBuf;

	return pos;
}

std::streambuf::pos_type ShadyCore::ZipStream::seekpos(pos_type spos, std::ios::openmode mode) {
	return seekoff(spos, std::ios::beg, mode);
}

void ShadyCore::ZipStream::open(const char* packageName, const char* innerName) {
	pkgFile = zip_open(packageName, ZIP_RDONLY, 0);

	zip_stat_t stat;
	zip_stat_init(&stat);
	zip_stat((zip_t*)pkgFile, innerName, 0, &stat);
	index = stat.index;
	size = stat.size;
	pos = 0;

	innerFile = zip_fopen_index((zip_t*)pkgFile, index, 0);
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

static inline ZipData* createZipEntry(ShadyCore::BasePackageEntry* entry) { return new ZipData{entry, 0}; }

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
		zipData->entry->close();
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

static void* zipProgressData = 0;
static void(*zipProgressDelegate)(void*, const char*, unsigned int, unsigned int) = 0;
static void zipProgress(double percent) {
	if (zipProgressDelegate) zipProgressDelegate(zipProgressData, "Compressing Zip", percent * 1000, 1000);
}

//-------------------------------------------------------------

int ShadyCore::Package::appendZipPackage(std::istream& input, const char* filename) {
	filename = allocateString(filename);

	BasePackageEntry* entry = new StreamPackageEntry(nextId, input, filename, std::filesystem::file_size(filename));
	zip_source_t* inputSource = zip_source_function_create(zipInputFunc, createZipEntry(entry), 0);
	zip_t* file = zip_open_from_source(inputSource, ZIP_RDONLY, 0);

	zip_int64_t count = zip_get_num_entries(file, 0);
	for (zip_int64_t i = 0; i < count; ++i) {
		zip_stat_t fileStat;
		zip_stat_index(file, i, 0, &fileStat);

		addOrReplace(new ZipPackageEntry(nextId, filename, allocateString(fileStat.name), fileStat.size));
	}

	zip_close(file);
	delete entry;
	return nextId++;
}

void ShadyCore::Package::saveZip(std::ostream& output, Callback* callback, void* userData) {
	zip_source_t* outputSource = zip_source_function_create(zipOutputFunc, &output, 0);
	zip_t* file = zip_open_from_source(outputSource, ZIP_CREATE | ZIP_TRUNCATE | ZIP_CHECKCONS, 0);

	for (auto entry : entries) {
		zip_source_t* inputSource = zip_source_function_create(zipInputFunc, createZipEntry(entry.second), 0);
		zip_file_add(file, entry.second->getName(), inputSource, ZIP_FL_OVERWRITE);
	}

	zipProgressDelegate = callback;
    zipProgressData = userData;
	zip_register_progress_callback(file, zipProgress);
	zip_close(file);
}

