#include "../Core/dataentry.hpp"
#include "../Core/package.hpp"
#include "../Core/util/tempfiles.hpp"
#include "util.hpp"
#include <fstream>
#include <gtest/gtest.h>

class FilterMock : public ShadyCore::StreamFilter {
public:
	int count = 0;
	inline FilterMock(std::streambuf* base, std::streamsize size) : ShadyCore::StreamFilter(base, size) { setg(0, 0, 0); }
	char_type filter(char_type c) override { ++count; return c; }
	void seek(off_type seek, std::ios::seekdir sdir) override { base->pubseekoff(seek, sdir, std::ios::in); }
};

class DataEntrySuite : public testing::Test {
public:
	static const char* dataArray[];
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

TEST_F(DataEntrySuite, StreamRead) {
	std::stringbuf sbuffer("Hello World!");
	FilterMock filter(&sbuffer, 12);
	std::istream stream(&filter);
	char buffer[5];

	// Get and Peek
	EXPECT_EQ(stream.get(buffer[0]).gcount(), 1);
	EXPECT_EQ(buffer[0], 'H');
	EXPECT_EQ(stream.peek(), 'e');
	EXPECT_EQ(filter.count, 2);

	// Get 4 bytes + '\0'
	EXPECT_EQ(stream.get(buffer, 5).gcount(), 4);
	EXPECT_EQ(strcmp(buffer, "ello"), 0);
	EXPECT_EQ(filter.count, 6); // This peek 1 ahead

	// Read 3 bytes
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 3);
	EXPECT_EQ(strncmp(buffer, " Wo", 3), 0);
	EXPECT_EQ(filter.count, 8);

	// Ignore 2 bytes and peek
	stream.ignore(2);
	//EXPECT_EQ(filter.count, 10); // There's diferences between libs
	EXPECT_EQ(stream.peek(), 'd');
	EXPECT_EQ(filter.count, 11);

	// Try read 5 but we have 2
	EXPECT_EQ(stream.read(buffer, 5).gcount(), 2);
	EXPECT_EQ(filter.count, 12);
}

TEST_F(DataEntrySuite, StreamWrite) {
	std::stringbuf sbuffer(std::ios::in | std::ios::out);
	FilterMock filter(&sbuffer, 0);
	std::ostream stream(&filter);

	// Put one byte
	stream.put('H');
	EXPECT_EQ(sbuffer.str().compare("H"), 0);
	EXPECT_EQ(filter.count, 1);

	// Write 4 bytes
	stream.write("ello", 4);
	EXPECT_EQ(sbuffer.str().compare("Hello"), 0);
	EXPECT_EQ(filter.count, 5);

	// Write cstring
	stream << " World!";
	EXPECT_EQ(sbuffer.str().compare("Hello World!"), 0);
	EXPECT_EQ(filter.count, 12);
}

TEST_F(DataEntrySuite, StreamSeek) {
	std::stringbuf sbuffer("Hello World!");
	FilterMock filter(&sbuffer, 12);
	std::istream stream(&filter);
	char buffer[5];

	// Read from start
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 3);
	EXPECT_EQ(strncmp(buffer, "Hel", 3), 0);

	// Seek back from start
	stream.seekg(1);
	EXPECT_EQ(stream.tellg(), (std::streampos) 1);
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 3);
	EXPECT_EQ(strncmp(buffer, "ell", 3), 0);

	// Seek back from current
	stream.seekg(-2, std::ios::cur);
	EXPECT_EQ(stream.tellg(), (std::streampos) 2);
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 3);
	EXPECT_EQ(strncmp(buffer, "llo", 3), 0);

	// Seek ahead from current
	stream.seekg(2, std::ios::cur);
	EXPECT_EQ(stream.tellg(), (std::streampos) 7);
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 3);
	EXPECT_EQ(strncmp(buffer, "orl", 3), 0);

	// Seek back from end
	stream.seekg(-6, std::ios::end);
	EXPECT_EQ(stream.tellg(), (std::streampos) 6);
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 3);
	EXPECT_EQ(strncmp(buffer, "Wor", 3), 0);

	// Seek ahead from end
	stream.seekg(-2, std::ios::end);
	EXPECT_EQ(stream.tellg(), (std::streampos) 10);
	EXPECT_EQ(stream.read(buffer, 3).gcount(), 2);
	EXPECT_EQ(strncmp(buffer, "d!r", 3), 0);
}

TEST_F(DataEntrySuite, ListFilter) {
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
		EXPECT_EQ(strcmp(name, data), 0);

		delete[] name;
	}
}

TEST_F(DataEntrySuite, PackageRead) {
	ShadyCore::Package package("test-data/data-package.dat");

	for (const char* data : dataArray) {
		std::istream& input = package.find(data)->second->open();
		EXPECT_TRUE(input.good());

		std::filesystem::path filename("test-data/encrypted"); filename /= data;
		std::ifstream expected(filename, std::ios::binary);
		EXPECT_TRUE(testing::isSameData(input, expected)) << "filename: " << data;

		package.find(data)->second->close();
		expected.close();
	}
}

TEST_F(DataEntrySuite, PackageWrite) {
	const char* inputList[] = {
		"test-data/encrypted",
		"test-data/decrypted",
		"test-data/data-package.dat",
		"test-data/zip-package.dat",
	};

	for (auto packageName : inputList) {
		ShadyCore::Package* input = new ShadyCore::Package(packageName);
		std::filesystem::path tempFile = ShadyUtil::TempFile();
		input->save(tempFile, ShadyCore::Package::DATA_MODE);
		ASSERT_TRUE(std::filesystem::exists(tempFile));
		delete input; input = new ShadyCore::Package(tempFile);

		ShadyCore::Package expected("test-data/data-package.dat");
		EXPECT_EQ(input->size(), expected.size());
		for (auto& file : expected) {
			std::string filename(file.first.name);
			file.first.fileType.appendExtValue(filename);
			auto i = input->find(filename);
			ASSERT_TRUE(i != input->end());

			std::istream& inputS = i->second->open();
			std::istream& expectedS = file.second->open();
			EXPECT_TRUE(testing::isSameData(inputS, expectedS)) << "filename: " << filename << ", package: " << packageName;
			file.second->close();
			i->second->close();
		}

		delete input;
		std::filesystem::remove(tempFile);
	}
}