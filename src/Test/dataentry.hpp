#include "../Core/dataentry.hpp"
#include "../Core/package.hpp"
#include "util.hpp"
#include <boost/filesystem.hpp>

class FilterMock : public ShadyCore::StreamFilter {
public:
	int count = 0;
	inline FilterMock(std::streambuf* base, std::streamsize size) : ShadyCore::StreamFilter(base, size) { setg(0, 0, 0); }
	char_type filter(char_type c) override { ++count; return c; }
	void seek(off_type seek, std::ios::seekdir sdir) override { base->pubseekoff(seek, sdir, std::ios::in); }
};

class DataEntrySuite : public CxxTest::TestSuite {
public:
	static const char* dataArray[];
	void testStreamRead();
	void testStreamWrite();
	void testStreamSeek();
	void testListFilter();
	void testPackageRead();
	void testPackageWrite();
};

const char* DataEntrySuite::dataArray[] = {
	"data/my-effect.pat",
	"data/my-gui.dat",
	"data/my-image-indexed.cv2",
	"data/my-image.cv2",
	"data/my-label.sfl",
	"data/my-palette.pal",
	"data/my-pattern.pat",
	"data/my-sfx.cv3",
	"data/my-text.cv0",
	"data/my-text.cv1",
};

void DataEntrySuite::testStreamRead() {
	std::stringbuf sbuffer("Hello World!");
	FilterMock filter(&sbuffer, 12);
	std::istream stream(&filter);
	char buffer[5];

	// Get and Peek
	TS_ASSERT_EQUALS(stream.get(buffer[0]).gcount(), 1);
	TS_ASSERT_EQUALS(buffer[0], 'H');
	TS_ASSERT_EQUALS(stream.peek(), 'e');
	TS_ASSERT_EQUALS(filter.count, 2);

	// Get 4 bytes + '\0'
	TS_ASSERT_EQUALS(stream.get(buffer, 5).gcount(), 4);
	TS_ASSERT_EQUALS(strcmp(buffer, "ello"), 0);
	TS_ASSERT_EQUALS(filter.count, 6); // This peek 1 ahead

	// Read 3 bytes
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, " Wo", 3), 0);
	TS_ASSERT_EQUALS(filter.count, 8);

	// Ignore 2 bytes and peek
	stream.ignore(2);
	//TS_ASSERT_EQUALS(filter.count, 10); // There's diferences between libs
	TS_ASSERT_EQUALS(stream.peek(), 'd');
	TS_ASSERT_EQUALS(filter.count, 11);

	// Try read 5 but we have 2
	TS_ASSERT_EQUALS(stream.read(buffer, 5).gcount(), 2);
	TS_ASSERT_EQUALS(filter.count, 12);
}

void DataEntrySuite::testStreamWrite() {
	std::stringbuf sbuffer(std::ios::in | std::ios::out);
	FilterMock filter(&sbuffer, 0);
	std::ostream stream(&filter);

	// Put one byte
	stream.put('H');
	TS_ASSERT_EQUALS(sbuffer.str().compare("H"), 0);
	TS_ASSERT_EQUALS(filter.count, 1);

	// Write 4 bytes
	stream.write("ello", 4);
	TS_ASSERT_EQUALS(sbuffer.str().compare("Hello"), 0);
	TS_ASSERT_EQUALS(filter.count, 5);

	// Write cstring
	stream << " World!";
	TS_ASSERT_EQUALS(sbuffer.str().compare("Hello World!"), 0);
	TS_ASSERT_EQUALS(filter.count, 12);
}

void DataEntrySuite::testStreamSeek() {
	std::stringbuf sbuffer("Hello World!");
	FilterMock filter(&sbuffer, 12);
	std::istream stream(&filter);
	char buffer[5];

	// Read from start
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, "Hel", 3), 0);

	// Seek back from start
	stream.seekg(1);
	TS_ASSERT_EQUALS(stream.tellg(), (std::streampos) 1);
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, "ell", 3), 0);

	// Seek back from current
	stream.seekg(-2, std::ios::cur);
	TS_ASSERT_EQUALS(stream.tellg(), (std::streampos) 2);
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, "llo", 3), 0);

	// Seek ahead from current
	stream.seekg(2, std::ios::cur);
	TS_ASSERT_EQUALS(stream.tellg(), (std::streampos) 7);
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, "orl", 3), 0);

	// Seek back from end
	stream.seekg(-6, std::ios::end);
	TS_ASSERT_EQUALS(stream.tellg(), (std::streampos) 6);
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, "Wor", 3), 0);

	// Seek ahead from end
	stream.seekg(-2, std::ios::end);
	TS_ASSERT_EQUALS(stream.tellg(), (std::streampos) 10);
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 2);
	TS_ASSERT_EQUALS(strncmp(buffer, "d!r", 3), 0);
}

void DataEntrySuite::testListFilter() {
	std::filebuf fbuf; fbuf.open("test-data/data-package.dat", std::ios::in | std::ios::binary);
	unsigned short fileCount; fbuf.sgetn((char*)&fileCount, 2);
	unsigned int listSize; fbuf.sgetn((char*)&listSize, 4);
	ShadyCore::DataListFilter listFilter(&fbuf, listSize + 6, listSize);

	std::istream input(&listFilter);
	for (const char* data : dataArray) {
		unsigned char nameSize;
		char* name;
		
		input.ignore(8);
		input.read((char*)&nameSize, 1);
		name = new char[nameSize + 1];
		name[nameSize] = '\0';
		input.read(name, nameSize);
		TS_ASSERT_EQUALS(strcmp(name, data), 0);

		delete[] name;
	}
}

void DataEntrySuite::testPackageRead() {
	ShadyCore::Package package;
	package.appendPackage("test-data/data-package.dat");

	for (const char* data : dataArray) {
		std::istream& input = package.findFile(data)->open();
		TS_ASSERT(input.good());

		boost::filesystem::path fileName("test-data/encrypted"); fileName /= data;
		boost::filesystem::ifstream expected(fileName, std::ios::binary);
		TS_ASSERT_STREAM(input, expected);

		package.findFile(data)->close();
		expected.close();
	}
}

void DataEntrySuite::testPackageWrite() {
	ShadyCore::Package package;
	package.appendPackage("test-data/encrypted");
	boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	package.save(tempFile.string().c_str(), ShadyCore::Package::DATA_MODE, 0);

	boost::filesystem::ifstream input(tempFile, std::ios::binary);
	TS_ASSERT(input.good());
	boost::filesystem::ifstream expected("test-data/data-package.dat", std::ios::binary);
	TS_ASSERT_STREAM(input, expected);

	input.close();
	expected.close();
	boost::filesystem::remove(tempFile);
}