#include "dataentry.hpp"
#include "fileentry.hpp"
#include "package.hpp"
#include "util/tempfiles.hpp"

#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <list>

#ifndef _MSC_VER
	using std::min;
	using std::max;
#endif

std::streamsize ShadyCore::StreamFilter::xsgetn(char_type* buffer, std::streamsize count) {
	int buffered = 0;
	if (hasBufVal && count) {
		--count;
		buffer[buffered++] = bufVal;
		hasBufVal = false;
	}
	pos += buffered;

	count = base->sgetn(buffer + buffered, std::min(count, (std::streamsize)(size - pos)));
	for (int i = buffered; i < count + buffered; ++i) {
		buffer[i] = filter(buffer[i]);
	}
	pos += count;

	return count + buffered;
}

std::streambuf::int_type ShadyCore::StreamFilter::underflow() {
	if (hasBufVal) return bufVal;

	int_type value = base->sbumpc();
	if (traits_type::eq_int_type(value, traits_type::eof())) return value;
	if (pos >= size) return traits_type::eof();
	hasBufVal = true;
	return bufVal = traits_type::to_int_type(filter(traits_type::to_char_type(value)));
}

std::streambuf::int_type ShadyCore::StreamFilter::uflow() {
	int_type value;
	if (hasBufVal) {
		++pos;
		hasBufVal = false;
		return bufVal;
	}

	value = base->sbumpc();
	if (traits_type::eq_int_type(value, traits_type::eof())) return value;
	if (pos >= size) return traits_type::eof();
	else ++pos;
	return traits_type::to_int_type(filter(traits_type::to_char_type(value)));
}

std::streamsize ShadyCore::StreamFilter::xsputn(const char_type* buffer, std::streamsize count) {
	char* tmp = new char[count];
	for (int i = 0; i < count; ++i) {
		tmp[i] = filter(buffer[i]);
	}
	count = base->sputn(tmp, count);
	delete[] tmp;
	return count;
}

std::streambuf::int_type ShadyCore::StreamFilter::overflow(int_type value) {
	if (traits_type::eq_int_type(value, traits_type::eof())) return value;
	return base->sputc(traits_type::to_int_type(filter(traits_type::to_char_type(value))));
}

std::streambuf::pos_type ShadyCore::StreamFilter::seekoff(off_type spos, std::ios::seekdir sdir, std::ios::openmode mode) {
	if (sdir == std::ios::cur && spos >= 0) {
		while (spos--) uflow();
		return pos;
	}

	seek(spos, sdir);
	if (sdir == std::ios::beg) pos = spos;
	if (sdir == std::ios::end) pos = size + spos;
	if (sdir == std::ios::cur) pos += spos;
	hasBufVal = false;

	return pos;
}

std::streambuf::pos_type ShadyCore::StreamFilter::seekpos(pos_type spos, std::ios::openmode mode) {
	return seekoff(spos, std::ios::beg, mode);
}

//-------------------------------------------------------------

void ShadyCore::DataListFilter::init() {
	key[0] = 0xC5;
	key[1] = 0x83;

	table[0] = seed & 0xffffffffUL;
	for (index = 1; index < TABLE_SIZE; ++index) {
		table[index] = (0x6C078965UL * (table[index - 1] ^ (table[index - 1] >> 30)) + index);
		table[index] &= 0xffffffffUL;
	}
}

std::streambuf::char_type ShadyCore::DataListFilter::randValue() {
	unsigned long rand;
	static unsigned long mag01[2] = { 0x0UL, MATRIX_A };

	if (index >= TABLE_SIZE) {
		int i;

		for (i = 0; i < TABLE_SIZE - TABLE_SHIFT; i++) {
			rand = (table[i] & UPPER_MASK) | (table[i + 1] & LOWER_MASK);
			table[i] = table[i + TABLE_SHIFT] ^ (rand >> 1) ^ mag01[rand & 0x1UL];
		}
		for (;i < TABLE_SIZE - 1; i++) {
			rand = (table[i] & UPPER_MASK) | (table[i + 1] & LOWER_MASK);
			table[i] = table[i + (TABLE_SHIFT - TABLE_SIZE)] ^ (rand >> 1) ^ mag01[rand & 0x1UL];
		}
		rand = (table[TABLE_SIZE - 1] & UPPER_MASK) | (table[0] & LOWER_MASK);
		table[TABLE_SIZE - 1] = table[TABLE_SHIFT - 1] ^ (rand >> 1) ^ mag01[rand & 0x1UL];

		index = 0;
	}

	rand = table[index++];

	rand ^= (rand >> 11);
	rand ^= (rand << 7) & 0x9d2c5680UL;
	rand ^= (rand << 15) & 0xefc60000UL;
	rand ^= (rand >> 18);

	return rand & 0xff;
}

std::streambuf::char_type ShadyCore::DataListFilter::filter(char_type value) {
	value ^= randValue();
	value ^= key[0];
	key[0] += key[1];
	key[1] += 0x53;
	return value;
}

void ShadyCore::DataListFilter::seek(off_type spos, std::ios::seekdir sdir) {
	if (sdir == std::ios::cur && spos >= 0) {
		base->pubseekoff(spos, sdir, std::ios::in);
		while (spos--) filter(0);
		return;
	}

	int position;
	if (sdir == std::ios::beg) position = spos;
	if (sdir == std::ios::cur) position = pos + spos;
	if (sdir == std::ios::end) position = size;

	base->pubseekoff(6 + position, std::ios::beg, std::ios::in);
}

void ShadyCore::DataFileFilter::seek(off_type spos, std::ios::seekdir sdir) {
	if (sdir == std::ios::cur) {
		if (hasBufVal) --spos;
		base->pubseekoff(spos, sdir, std::ios::in);
	} else if (sdir == std::ios::beg) {
		base->pubseekoff(spos + offset, sdir, std::ios::in);
	} else if (sdir == std::ios::end) {
		base->pubseekoff(spos + size + offset, std::ios::beg, std::ios::in);
	}
}

//-------------------------------------------------------------
namespace {
	struct StreamData {
		std::istream fileStream; // must be first
		std::filebuf baseBuffer;
		ShadyCore::DataFileFilter fileFilter;
		inline StreamData(unsigned int offset, unsigned int size)
			: fileStream(&fileFilter), fileFilter(&baseBuffer, offset, size) {}
	};
}


std::istream& ShadyCore::DataPackageEntry::open() {
	++openCount;
	auto streamData = new StreamData(packageOffset, packageSize);

#ifdef _MSC_VER
	streamData->baseBuffer.open(parent->getBasePath(), std::ios::in | std::ios::binary, _SH_DENYWR);
#else
	streamData->baseBuffer.open(parent->getBasePath(), std::ios::in | std::ios::binary);
#endif
	streamData->baseBuffer.pubseekoff(packageOffset, std::ios::beg);

	return streamData->fileStream;
}

void ShadyCore::DataPackageEntry::close(std::istream& fileStream) {
	delete (StreamData*)&fileStream;
	if (openCount > 0) --openCount;
	if (disposable && !openCount) delete this;
}

//-------------------------------------------------------------

void ShadyCore::Package::loadData(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);

	unsigned short fileCount; input.read((char*)&fileCount, 2);
	unsigned int listSize; input.read((char*)&listSize, 4);
	DataListFilter inputFilter(input.rdbuf(), listSize + 6, listSize);
	std::istream filteredInput(&inputFilter);

	entries.reserve(entries.size() + fileCount);
	for (int i = 0; i < fileCount; ++i) {
		unsigned int offset, size;
		unsigned char nameSize;

		filteredInput.read((char*)&offset, 4);
		filteredInput.read((char*)&size, 4);
		filteredInput.read((char*)&nameSize, 1);
		std::string name; name.resize(nameSize);
		filteredInput.read(name.data(), nameSize);

		this->insert(name, new DataPackageEntry(this, offset, size));
	}
}

namespace {
	using FT = ShadyCore::FileType;
	constexpr FT outputTypes[] = {
		FT(FT::TYPE_UNKNOWN, FT::FORMAT_UNKNOWN),
		FT(FT::TYPE_TEXT, FT::TEXT_GAME, FT::getExtValue(".cv0")),
		FT(FT::TYPE_TABLE, FT::TABLE_GAME, FT::getExtValue(".cv1")),
		FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl")),
		FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2")),
		FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal")),
		FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3")),
		FT(FT::TYPE_BGM, FT::BGM_OGG, FT::getExtValue(".ogg")),
		FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN),
		FT(FT::TYPE_TEXTURE, FT::TEXTURE_DDS, FT::getExtValue(".dds"))
	};
}

ShadyCore::FileType ShadyCore::GetDataPackageDefaultType(const FT& inputType, ShadyCore::BasePackageEntry* entry) {
	if (inputType == FT::TYPE_SCHEMA) {
		FT::Format format = FT::FORMAT_UNKNOWN;
		if (inputType.format == FT::SCHEMA_XML) {
			char buffer[16];
			std::istream& input = entry->open();
			// read format from the first xml tag
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
	} else return outputTypes[inputType.type];
}

ShadyCore::FileType ShadyCore::GetDataPackageDefaultType(const FT::Type& inputType, ShadyCore::Resource* resource) {
	if (inputType == FT::TYPE_SCHEMA) {
		auto schema = reinterpret_cast<ShadyCore::Schema*>(resource);
		FT::Format format = FT::FORMAT_UNKNOWN;
		if (schema->objects.size()) {
			switch(schema->objects[0]->getType()) {
				case 9: format = FT::SCHEMA_GAME_PATTERN; break;
				case 8: format = FT::SCHEMA_GAME_ANIM; break;
				default: format = FT::SCHEMA_GAME_GUI; break;
			}
		} return FT(FT::TYPE_SCHEMA, format, format == FT::SCHEMA_GAME_GUI ? FT::getExtValue(".dat") : FT::getExtValue(".pat"));
	} else return outputTypes[inputType];
}

void ShadyCore::Package::saveData(const std::filesystem::path& filename) {
	std::shared_lock lock(*this);
	std::list<std::pair<std::string, std::filesystem::path> > tempFiles;
	unsigned short fileCount = entries.size();
	unsigned int listSize = 0;
	for (auto i = begin(); i != end(); ++i) {
		auto entry = i->second;
		FileType inputType = i.fileType();
		FileType targetType = GetDataPackageDefaultType(inputType, entry);
		tempFiles.emplace_back(i->first.name, ShadyUtil::TempFile());

		std::ofstream output(tempFiles.back().second, std::ios::binary);
		std::istream& input = entry->open();

		ShadyCore::convertResource(inputType.type, inputType.format, input, targetType.format, output);

		entry->close(input);
		Package::underlineToSlash(tempFiles.back().first);
		targetType.appendExtValue(tempFiles.back().first);
		listSize += 9 + tempFiles.back().first.size();
	}

	std::ofstream output(filename, std::ios::binary);
	output.write((char*)&fileCount, 2);
	output.write((char*)&listSize, 4);
	std::ostream headerOutput(new DataListFilter(output.rdbuf(), listSize + 6, listSize));
	unsigned int curOffset = listSize + 6;
	for (auto& file : tempFiles) {
		uint32_t size = std::filesystem::file_size(file.second);
		headerOutput.write((char*)&curOffset, 4);
		headerOutput.write((char*)&size, 4);
		headerOutput.put(file.first.size());
		headerOutput.write(file.first.c_str(), file.first.size());

		curOffset += size;
	} delete headerOutput.rdbuf();

	char buffer[4096];
	curOffset = listSize + 6;
	for (auto& file : tempFiles) {
		uint32_t size = std::filesystem::file_size(file.second);
		std::ostream fileOutput(new DataFileFilter(output.rdbuf(), curOffset, size));
		std::ifstream input(file.second, std::ios::binary);

		fileOutput << input.rdbuf();

		input.close(); std::filesystem::remove(file.second);
		curOffset += size;
		delete fileOutput.rdbuf();
	}
}
