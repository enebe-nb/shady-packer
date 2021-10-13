#include "../Core/package.hpp"
#include "../Core/util/tempfiles.hpp"
#include "util.hpp"
#include <fstream>
#include <gtest/gtest.h>

using filePair = std::pair<const char*, const char*>;

class ReaderWriterSuite : public ::testing::TestWithParam<filePair> {
public:
	static filePair dataArray[];
};

filePair ReaderWriterSuite::dataArray[] = {
	{ "test-data/encrypted/data/my-text.cv0", "test-data/decrypted/data/my-text.txt" },
	{ "test-data/encrypted/data/my-text.cv1", "test-data/decrypted/data/my-text.csv" },
	{ "test-data/encrypted/data/my-label.sfl", "test-data/decrypted/data/my-label.lbl" },
	{ "test-data/encrypted/data/my-image.cv2", "test-data/decrypted/data/my-image.png" },
	{ "test-data/encrypted/data/my-image-indexed.cv2", "test-data/decrypted/data/my-image-indexed.png" },
	{ "test-data/encrypted/data/my-palette.pal", "test-data/decrypted/data/my-palette.act" },
	{ "test-data/encrypted/data/my-sfx.cv3", "test-data/decrypted/data/my-sfx.wav" },
	{ "test-data/encrypted/data/my-gui.dat", "test-data/decrypted/data/my-gui.xml" },
	{ "test-data/encrypted/data/my-pattern.pat", "test-data/decrypted/data/my-pattern.xml" },
	{ "test-data/encrypted/data/my-effect.pat", "test-data/decrypted/data/my-effect.xml" }
};;

TEST_P(ReaderWriterSuite, Convertion) {
	auto data = GetParam();
	for (int i = 0; i < 2; ++i) {
		const char* current = i==0 ? data.first : data.second;
		std::ifstream input(current, std::ios::binary), expected;
		ShadyCore::Resource* resource = ShadyCore::readResource(ShadyCore::FileType::get(current, input), input);

		std::stringstream output;
		ShadyCore::writeResource(resource, output, true);
		expected.open(data.first, std::ios::binary);
		ASSERT_STREAM(output, expected);
		expected.close();

		output.clear(); output.str("");
		ShadyCore::writeResource(resource, output, false);
		expected.open(data.second, std::ios::binary);
		ASSERT_STREAM(output, expected);
		expected.close();

		delete resource;
		input.close();
	}
}

TEST_P(ReaderWriterSuite, PackageRead) {
	auto data = GetParam();

	std::filesystem::path zipFile = ShadyUtil::TempFile();
	std::filesystem::path dataFile = ShadyUtil::TempFile();
	ShadyCore::Package package;
	package.appendFile(data.first, data.first);
	package.appendFile(data.second, data.second);
	package.save(zipFile.string().c_str(), ShadyCore::Package::ZIP_MODE, 0, 0);
	package.save(dataFile.string().c_str(), ShadyCore::Package::DATA_MODE, 0, 0);
	package.clear();

	for (auto file : { zipFile, dataFile }) {
		ShadyCore::Resource* resource;
		package.appendPackage(file.string().c_str());
		std::istream &input0 = package.findFile(data.first)->open(), &input1 = package.findFile(data.second)->open();
		const ShadyCore::FileType &type0 = ShadyCore::FileType::get(data.first, input0), &type1 = ShadyCore::FileType::get(data.second, input1);

		std::stringstream output; std::ifstream expected;
		ShadyCore::writeResource(resource = ShadyCore::readResource(type0, input0), output, type0.isEncrypted);
		expected.open(data.first, std::ios::binary);
		ASSERT_STREAM(output, expected);
		expected.close();
		delete resource;

		output.clear(); output.str("");
		ShadyCore::writeResource(resource = ShadyCore::readResource(type1, input1), output, type1.isEncrypted);
		expected.open(data.second, std::ios::binary);
		ASSERT_STREAM(output, expected);
		expected.close();
		delete resource;

		package.findFile(data.first)->close();
		package.findFile(data.second)->close();
		package.clear();
	}

	std::filesystem::remove(zipFile);
	std::filesystem::remove(dataFile);
}

INSTANTIATE_TEST_SUITE_P(FileList, ReaderWriterSuite, ::testing::ValuesIn(ReaderWriterSuite::dataArray) );
