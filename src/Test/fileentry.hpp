#include "../Core/fileentry.hpp"
#include "../Core/package.hpp"
#include "util.hpp"

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
	ShadyCore::FilePackageEntry entry("data/my-text.txt", "test-data/decrypted/data/my-text.txt");
	std::istream& input = entry.open();
	std::ifstream expected("test-data/decrypted/data/my-text.txt");

	TS_ASSERT_STREAM(input, expected);

	entry.close();
	expected.close();
}

void FileEntrySuite::testFileTemporary() {
	boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	boost::filesystem::copy_file("test-data/decrypted/data/my-text.txt", tempFile);
	TS_ASSERT(boost::filesystem::exists(tempFile));

	char buffer[255]; strcpy(buffer, tempFile.string().c_str());
	ShadyCore::FilePackageEntry* entry = new ShadyCore::FilePackageEntry("data/my-text.txt", buffer, true);
	std::istream& input = entry->open();
	std::ifstream expected("test-data/decrypted/data/my-text.txt");
	TS_ASSERT_STREAM(input, expected);
	entry->close();

	delete entry;
	TS_ASSERT(!boost::filesystem::exists(tempFile));
}

void FileEntrySuite::testPackageRead() {
	ShadyCore::Package package;
	package.appendPackage("test-data/encrypted");

	for (const char* data : dataArray) {
		std::istream& input = package.findFile(data)->open();
		boost::filesystem::path fileName("test-data/encrypted"); fileName /= data;
		boost::filesystem::ifstream expected(fileName, std::ios::binary);

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

	boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	package.save(tempFile.string().c_str(), ShadyCore::Package::DIR_MODE, 0);

	for (boost::filesystem::recursive_directory_iterator iter("test-data/decrypted"), e; iter != e; ++iter) {
		boost::filesystem::path fileName = tempFile / boost::filesystem::relative(iter->path(), "test-data/decrypted");

		TS_ASSERT(boost::filesystem::exists(fileName));
		if (boost::filesystem::is_regular_file(fileName)) {
			boost::filesystem::ifstream input(fileName, std::ios::binary);
			boost::filesystem::ifstream expected(iter->path(), std::ios::binary);

			TS_ASSERT_STREAM(input, expected);

			input.close();
			expected.close();
		} else {
			TS_ASSERT(!boost::filesystem::is_regular_file(iter->path()));
		}
	}

	boost::filesystem::remove_all(tempFile);
}