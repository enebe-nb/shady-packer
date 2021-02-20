#include "../Core/fileentry.hpp"
#include "../Core/package.hpp"
#include "util.hpp"
#include <filesystem>
#include <fstream>

class FileEntrySuite : public CxxTest::TestSuite {
public:
	static const char* dataArray[];
	void testFileRead();
	void testFileTemporary();
	void testPackageRead();
	void testPackageAppend();
	void testPackageWrite();
};

const char* FileEntrySuite::dataArray[] = {
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

void FileEntrySuite::testFileRead() {
	ShadyCore::FilePackageEntry entry(0, "data/my-text.txt", "test-data/decrypted/data/my-text.txt");
	std::istream& input = entry.open();
	std::ifstream expected("test-data/decrypted/data/my-text.txt");

	TS_ASSERT_STREAM(input, expected);

	entry.close();
	expected.close();
}

void FileEntrySuite::testFileTemporary() {
	std::filesystem::path tempFile = ShadyCore::TempFile();
	std::filesystem::copy_file("test-data/decrypted/data/my-text.txt", tempFile);
	TS_ASSERT(std::filesystem::exists(tempFile));

	char buffer[255]; strcpy(buffer, tempFile.string().c_str());
	ShadyCore::FilePackageEntry* entry = new ShadyCore::FilePackageEntry(0, "data/my-text.txt", buffer, true);
	std::istream& input = entry->open();
	std::ifstream expected("test-data/decrypted/data/my-text.txt");
	TS_ASSERT_STREAM(input, expected);
	entry->close();

	delete entry;
	TS_ASSERT(!std::filesystem::exists(tempFile));
}

void FileEntrySuite::testPackageRead() {
	ShadyCore::Package package;
	package.appendPackage("test-data/encrypted");

	for (const char* data : dataArray) {
		std::istream& input = package.findFile(data)->open();
		std::filesystem::path fileName("test-data/encrypted"); fileName /= data;
		std::ifstream expected(fileName, std::ios::binary);

		TS_ASSERT_STREAM(input, expected);

		package.findFile(data)->close();
		expected.close();
	}
}

void FileEntrySuite::testPackageAppend() {
	ShadyCore::Package package;
	package.appendFile("file-test", "test-data/decrypted/data/my-text.txt");
    std::ifstream input("test-data/decrypted/data/my-text.txt", std::ios::binary);
	package.appendFile("stream-test", input);

	std::ifstream expected("test-data/decrypted/data/my-text.txt", std::ios::binary);
	std::istream& fileInput = package.findFile("file-test")->open();
	TS_ASSERT_STREAM(fileInput, expected);
	package.findFile("file-test")->close();

	expected.clear(); expected.seekg(0);
	std::istream& streamInput = package.findFile("stream-test")->open();
	TS_ASSERT_STREAM(streamInput, expected);

	package.findFile("stream-test")->close();
	expected.close();
}

void FileEntrySuite::testPackageWrite() {
	ShadyCore::Package package;
	package.appendPackage("test-data/decrypted");

	std::filesystem::path tempFile = ShadyCore::TempFile();
	package.save(tempFile.string().c_str(), ShadyCore::Package::DIR_MODE, 0, 0);

	for (std::filesystem::recursive_directory_iterator iter("test-data/decrypted"), e; iter != e; ++iter) {
		std::filesystem::path fileName = tempFile / std::filesystem::relative(iter->path(), "test-data/decrypted");

		TS_ASSERT(std::filesystem::exists(fileName));
		if (std::filesystem::is_regular_file(fileName)) {
			std::ifstream input(fileName, std::ios::binary);
			std::ifstream expected(iter->path(), std::ios::binary);

			TS_ASSERT_STREAM(input, expected);

			input.close();
			expected.close();
		} else {
			TS_ASSERT(!std::filesystem::is_regular_file(iter->path()));
		}
	}

	std::filesystem::remove_all(tempFile);
}