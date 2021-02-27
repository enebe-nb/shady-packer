#include "../Core/zipentry.hpp"
#include "../Core/package.hpp"
#include "util.hpp"
#include "../Core/util/tempfiles.hpp"
#include <filesystem>
#include <fstream>

class ZipEntrySuite : public CxxTest::TestSuite {
public:
	static const char* dataArray[];
	void testStreamRead();
	//void testStreamWrite(); // TODO write into zip (maybe?)
	void testStreamSeek();
	void testPackageRead();
	void testPackageWrite();
};

const char* ZipEntrySuite::dataArray[] = {
	"data_my-effect.pat",
	"data_my-gui.dat",
	"data_my-image.png",
	"data_my-image-indexed.png",
	"data_my-label.sfl",
	"data_my-palette.pal",
	"data_my-pattern.pat",
	"data_my-sfx.cv3",
	"data_my-text.cv0",
	"data_my-text.cv1",
};

void ZipEntrySuite::testStreamRead() {
	ShadyCore::ZipPackageEntry entry(0, "test-data/zip-package.dat", "data_my-text.cv0", 12);
	std::istream& stream = entry.open();
	char buffer[5];

	// Get and Peek
	TS_ASSERT_EQUALS(stream.get(buffer[0]).gcount(), 1);
	TS_ASSERT_EQUALS(buffer[0], 'H');
	TS_ASSERT_EQUALS(stream.peek(), 'e');

	// Get 4 bytes + '\0'
	TS_ASSERT_EQUALS(stream.get(buffer, 5).gcount(), 4);
	TS_ASSERT_EQUALS(strcmp(buffer, "ello"), 0);

	// Read 3 bytes
	TS_ASSERT_EQUALS(stream.read(buffer, 3).gcount(), 3);
	TS_ASSERT_EQUALS(strncmp(buffer, " Wo", 3), 0);

	// Ignore 2 bytes and peek
	stream.ignore(2);
	TS_ASSERT_EQUALS(stream.peek(), 'd');

	// Try read 5 but we have 2
	TS_ASSERT_EQUALS(stream.read(buffer, 5).gcount(), 2);

	entry.close();
}

void ZipEntrySuite::testStreamSeek() {
	ShadyCore::ZipPackageEntry entry(0, "test-data/zip-package.dat", "data_my-text.cv0", 12);
	std::istream& stream = entry.open();
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

void ZipEntrySuite::testPackageRead() {
	ShadyCore::Package package;
	package.appendPackage("test-data/zip-package.dat");

	for (const char* data : dataArray) {
		std::istream& input = package.findFile(data)->open();
		std::filesystem::path fileName("test-data/zip-extracted"); fileName /= data;
		std::ifstream expected(fileName, std::ios::binary);

		TS_ASSERT_STREAM(input, expected);

		package.findFile(data)->close();
		expected.close();
	}
}

void ZipEntrySuite::testPackageWrite() {
	std::filesystem::path tempFile = ShadyUtil::TempFile();
	ShadyCore::Package package;
	package.appendPackage("test-data/zip-extracted");
	package.save(tempFile.string().c_str(), ShadyCore::Package::ZIP_MODE, 0, 0);
	package.clear();

	ShadyCore::Package expectedPackage;
	package.appendPackage(tempFile.string().c_str());
	expectedPackage.appendPackage("test-data/zip-package.dat");

	TS_ASSERT_EQUALS(package.size(), 10);
	for (auto& entry : package) {
		std::istream& input = entry.open();
		std::istream& expected = expectedPackage.findFile(entry.getName())->open();

		TS_ASSERT_STREAM(input, expected);

		entry.close();
		expectedPackage.findFile(entry.getName())->close();
	}

	std::filesystem::remove(tempFile);
}