#include "../Core/package.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "util.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cinttypes>

class ReaderWriterSuite : public CxxTest::TestSuite {
public:
	static const char* dataArray[][2];
	void testConvertion();
	void testPackageRead();
};

const char* ReaderWriterSuite::dataArray[][2] = {
	{ "test-data/encrypted/data/my-text.cv0", "test-data/decrypted/data/my-text.txt" },
	{ "test-data/encrypted/data/my-text.cv1", "test-data/decrypted/data/my-text.csv" },
	{ "test-data/encrypted/data/my-label.sfl", "test-data/decrypted/data/my-label.lbl" },
	{ "test-data/encrypted/data/my-image.cv2", "test-data/decrypted/data/my-image.png" },
	{ "test-data/encrypted/data/my-image-indexed.cv2", "test-data/decrypted/data/my-image-indexed.png" },
	{ "test-data/encrypted/data/my-palette.pal", "test-data/decrypted/data/my-palette.act" },
	{ "test-data/encrypted/data/my-sfx.cv3", "test-data/decrypted/data/my-sfx.wav" },
	{ "test-data/encrypted/data/my-gui.dat", "test-data/decrypted/data/my-gui.xml" },
	{ "test-data/encrypted/data/my-pattern.pat", "test-data/decrypted/data/my-pattern.xml" },
	{ "test-data/encrypted/data/my-effect.pat", "test-data/decrypted/data/my-effect.xml" },
};

void ReaderWriterSuite::testConvertion() {
	for (auto data : dataArray) {
		for (int i = 0; i < 2; ++i) {
			std::ifstream input(data[i], std::ios::binary), expected;
			ShadyCore::Resource* resource = ShadyCore::readResource(ShadyCore::FileType::get(data[i], input), input);

			std::stringstream output;
			ShadyCore::writeResource(resource, output, true);
			expected.open(data[0], std::ios::binary);
			TS_ASSERT_STREAM(output, expected);
			expected.close();

			output.clear(); output.str("");
			ShadyCore::writeResource(resource, output, false);
			expected.open(data[1], std::ios::binary);
			TS_ASSERT_STREAM(output, expected);
			expected.close();

			delete resource;
			input.close();
		}
	}
}

void ReaderWriterSuite::testPackageRead() {
	for (auto data : dataArray) {
		std::filesystem::path zipFile = std::filesystem::temp_directory_path() / std::tmpnam(nullptr);
		std::filesystem::path dataFile = std::filesystem::temp_directory_path() / std::tmpnam(nullptr);
		ShadyCore::Package package;
		package.appendFile(data[0], data[0]);
		package.appendFile(data[1], data[1]);
		package.save(zipFile.string().c_str(), ShadyCore::Package::ZIP_MODE, 0, 0);
		package.save(dataFile.string().c_str(), ShadyCore::Package::DATA_MODE, 0, 0);
		package.clear();

		for (auto file : { zipFile, dataFile }) {
			ShadyCore::Resource* resource;
			package.appendPackage(file.string().c_str());
			std::istream &input0 = package.findFile(data[0])->open(), &input1 = package.findFile(data[1])->open();
			const ShadyCore::FileType &type0 = ShadyCore::FileType::get(data[0], input0), &type1 = ShadyCore::FileType::get(data[1], input1);

			std::stringstream output; std::ifstream expected;
			ShadyCore::writeResource(resource = ShadyCore::readResource(type0, input0), output, type0.isEncrypted);
			expected.open(data[0], std::ios::binary);
			TS_ASSERT_STREAM(output, expected);
			expected.close();
			delete resource;

			output.clear(); output.str("");
			ShadyCore::writeResource(resource = ShadyCore::readResource(type1, input1), output, type1.isEncrypted);
			expected.open(data[1], std::ios::binary);
			TS_ASSERT_STREAM(output, expected);
			expected.close();
			delete resource;

			package.findFile(data[0])->close();
			package.findFile(data[1])->close();
			package.clear();
		}

		std::filesystem::remove(zipFile);
		std::filesystem::remove(dataFile);
	}
}