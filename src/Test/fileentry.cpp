#include "../Core/fileentry.hpp"
#include "../Core/package.hpp"
#include "../Core/util/tempfiles.hpp"
#include "util.hpp"
#include <fstream>
#include <gtest/gtest.h>

class FileEntrySuite : public testing::Test {
public:
	static const char* dataArray[];
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

TEST_F(FileEntrySuite, FileTemporary) {
	std::filesystem::path tempFile = ShadyUtil::TempFile();
	std::filesystem::copy_file("test-data/decrypted/data/my-text.txt", tempFile);
	EXPECT_TRUE(std::filesystem::exists(tempFile));

	ShadyCore::PackageEx t("K:/anything/that should/not/exist");
	ShadyCore::FilePackageEntry* entry = new ShadyCore::FilePackageEntry(&t, tempFile, true);
	std::istream& input = entry->open();
	std::ifstream expected("test-data/decrypted/data/my-text.txt");
	EXPECT_TRUE(testing::isSameData(input, expected));
	entry->close(input);

	delete entry;
	EXPECT_TRUE(!std::filesystem::exists(tempFile));
}

TEST_F(FileEntrySuite, PackageRead) {
	ShadyCore::Package package("test-data/encrypted");

	for (const char* data : dataArray) {
		std::istream& input = package.find(data)->second->open();
		std::filesystem::path filename("test-data/encrypted"); filename /= data;
		std::ifstream expected(filename, std::ios::binary);

		EXPECT_TRUE(testing::isSameData(input, expected)) << "filename: " << data;

		package.find(data)->second->close(input);
		expected.close();
	}
}

TEST_F(FileEntrySuite, PackageWrite) {
	const char* inputList[] = {
		"test-data/encrypted",
		"test-data/decrypted",
		"test-data/data-package.dat",
		"test-data/zip-package.dat",
	};

	for (auto packageName : inputList) {
		ShadyCore::Package* input = new ShadyCore::Package(packageName);
		std::filesystem::path tempFile = ShadyUtil::TempFile();
		input->save(tempFile, ShadyCore::Package::DIR_MODE);
		ASSERT_TRUE(std::filesystem::exists(tempFile) && std::filesystem::is_directory(tempFile));
		delete input; input = new ShadyCore::Package(tempFile);

		for (std::filesystem::recursive_directory_iterator iter("test-data/decrypted"), e; iter != e; ++iter) {
			std::wstring filename = std::filesystem::relative(iter->path(), "test-data/decrypted");
			std::replace(filename.begin(), filename.end(), L'\\', L'_');
			filename = tempFile / filename;

			if (std::filesystem::is_regular_file(filename)) {
				std::ifstream input(filename, std::ios::binary);
				std::ifstream expected(iter->path(), std::ios::binary);

				EXPECT_TRUE(testing::isSameData(input, expected)) << "filename: " << filename << ", package: " << packageName;

				input.close();
				expected.close();
			} else {
				EXPECT_TRUE(std::filesystem::is_directory(iter->path()));
			}
		}

		std::filesystem::remove_all(tempFile);
	}
}