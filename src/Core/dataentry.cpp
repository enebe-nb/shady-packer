#include "dataentry.hpp"
#include "package.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstring>

std::streamsize ShadyCore::StreamFilter::xsgetn(char_type* buffer, std::streamsize count) {
	int buffered = 0;
	while (!pool.empty() && count) {
		--count;
		buffer[buffered++] = pool.top();
		pool.pop();
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
	if (!pool.empty()) return pool.top();

	int_type value = base->sbumpc();
	if (traits_type::eq_int_type(value, traits_type::eof())) return value;
	if (pos >= size) return traits_type::eof();
	pool.push(traits_type::to_int_type(filter(traits_type::to_char_type(value))));
	return pool.top();
}

std::streambuf::int_type ShadyCore::StreamFilter::uflow() {
	int_type value;
	if (!pool.empty()) {
		++pos;
		value = pool.top();
		pool.pop();
		return value;
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
	while (!pool.empty()) pool.pop();

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
		spos -= pool.size();
		base->pubseekoff(spos, sdir, std::ios::in);
	} else if (sdir == std::ios::beg) {
		base->pubseekoff(spos + offset, sdir, std::ios::in);
	} else if (sdir == std::ios::end) {
		base->pubseekoff(spos + size + offset, std::ios::beg, std::ios::in);
	}
}

//-------------------------------------------------------------

std::istream& ShadyCore::DataPackageEntry::open() {
	fileStream.clear();
	std::filebuf* base = (std::filebuf*) fileFilter.getBaseBuffer();
	if (!base) base = new std::filebuf();
	else base->close();

	base->open(packageFilename, std::ios::in | std::ios::binary);
	base->pubseekoff(packageOffset, std::ios::beg);
	fileFilter.setBaseBuffer(base);

	return fileStream;
}

void ShadyCore::DataPackageEntry::close() {
	const std::streambuf* base = fileFilter.getBaseBuffer();
	if (base) {
		delete base;
		fileFilter.setBaseBuffer(0);
	}
}

//-------------------------------------------------------------

int ShadyCore::Package::appendDataPackage(std::istream& input, const char* filename) {
	filename = allocateString(filename);

	unsigned short fileCount; input.read((char*)&fileCount, 2);
	unsigned int listSize; input.read((char*)&listSize, 4);
	DataListFilter inputFilter(input.rdbuf(), listSize + 6, listSize);
	std::istream filteredInput(&inputFilter);

	for (int i = 0; i < fileCount; ++i) {
		unsigned int offset, size;
		unsigned char nameSize;
		char* name;

		filteredInput.read((char*)&offset, 4);
		filteredInput.read((char*)&size, 4);
		filteredInput.read((char*)&nameSize, 1);
		name = allocateString(nameSize);
		filteredInput.read(name, nameSize);

		addOrReplace(new DataPackageEntry(nextId, filename, name, offset, size));
	}
	return nextId++;
}

void ShadyCore::Package::saveData(std::ostream& output, Callback* callback, void* userData) {
	unsigned short fileCount = entries.size();
	unsigned int listSize = 0;
	for (auto entry : entries) {
		listSize += 9 + entry.first.size();
	}

	output.write((char*)&fileCount, 2);
	output.write((char*)&listSize, 4);
	std::ostream headerOutput(new DataListFilter(output.rdbuf(), listSize + 6, listSize));
	unsigned int curOffset = listSize + 6;
	for (auto entry : entries) {
		unsigned char len = entry.first.size();
		headerOutput.write((char*)&curOffset, 4);
		headerOutput.write((char*)&entry.second->getSize(), 4);
		headerOutput.put(len);
		headerOutput.write(std::filesystem::path(entry.first).generic_string().c_str(), len);

		curOffset += entry.second->getSize();
	} delete headerOutput.rdbuf();

	char buffer[4096];
	unsigned int index = 0;
	curOffset = listSize + 6;
	for (auto entry : entries) {
		if (callback) callback(userData, entry.second->getName(), ++index, fileCount);
		std::ostream fileOutput(new DataFileFilter(output.rdbuf(), curOffset, entry.second->getSize()));
		std::istream& input = entry.second->open();

		int read;
		while (read = input.read(buffer, 4096).gcount())
			fileOutput.write(buffer, read);

		entry.second->close();
		curOffset += entry.second->getSize();
		delete fileOutput.rdbuf();
	}
}
