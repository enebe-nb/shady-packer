#include "../Core/package.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "util.hpp"
#include <boost/filesystem.hpp>
 
class PackageSuite : public CxxTest::TestSuite {
public:
	void testStreamEntry();
	void testReplaceEntry();
	void testFileType();
	// TODO complete tests
};

void PackageSuite::testStreamEntry() {
	ShadyCore::Package package;
	std::stringstream entryBase("Hello World!");
	ShadyCore::StreamPackageEntry entry(entryBase, "stream-entry", 12);
	char buffer[15];

	TS_ASSERT_EQUALS(entry.open().read(buffer, 15).gcount(), 12);
	TS_ASSERT_EQUALS(strncmp(buffer, "Hello World!", 12), 0);

	entry.close();
}

void PackageSuite::testReplaceEntry() {
	ShadyCore::Package package;
	package.appendFile("data/my-text.txt", "test-data/decrypted/data/my-pattern.xml");
	package.appendFile("data/my-text.txt", "test-data/decrypted/data/my-text.txt");

	char buffer[13];
	TS_ASSERT_EQUALS(package.findFile("data/my-text.txt")->open().read(buffer, 13).gcount(), 12);
	buffer[12] = '\0';
	TS_ASSERT_EQUALS(strcmp(buffer, "Hello World!"), 0);
	package.findFile("data/my-text.txt")->close();

	package.appendPackage("test-data/data-package.dat");
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE);
	package.appendPackage("test-data/zip-package.dat");

	TS_ASSERT_EQUALS(package.findFile("data_my-text.cv0")->open().read(buffer, 13).gcount(), 12);
	buffer[12] = '\0';
	TS_ASSERT_EQUALS(strcmp(buffer, "Hello World!"), 0);
	package.findFile("data_my-text.cv0")->close();
}

void PackageSuite::testFileType() {
	std::stringstream guiFile(std::string("\x04\x00\x00\x00", 4));
	std::stringstream otherFile(std::string("\x04\x00\x05\x00", 4));

	const ShadyCore::FileType& txtType = ShadyCore::FileType::get("data/my-text.txt", guiFile);
	TS_ASSERT_EQUALS(txtType, ShadyCore::FileType::TYPE_TEXT);
	TS_ASSERT_EQUALS(txtType.isEncrypted, false);

	const ShadyCore::FileType& guiType = ShadyCore::FileType::get("data/my-gui.dat", guiFile);
	TS_ASSERT_EQUALS(guiType, ShadyCore::FileType::TYPE_GUI);
	TS_ASSERT_EQUALS(guiType.isEncrypted, true);
	unsigned int version; guiFile.read((char*)&version, 4);
	TS_ASSERT_EQUALS(version, 4);

	const ShadyCore::FileType& packageType = ShadyCore::FileType::get("data/my-gui.dat", otherFile);
	TS_ASSERT_EQUALS(packageType, ShadyCore::FileType::TYPE_PACKAGE);

	const ShadyCore::FileType& unknownType = ShadyCore::FileType::get("data/my-unknown.typ", guiFile);
	TS_ASSERT_EQUALS(unknownType, ShadyCore::FileType::TYPE_UNKNOWN);

	// TODO All types
}

class FilterSuite : public CxxTest::TestSuite {
public:
	void testUnderline();
	void testZipText();
	void testZipConvert() {/* TODO */}
	void testDecrypt();
	void testEncrypt();
};

void FilterSuite::testUnderline() {
	ShadyCore::Package package;
	const char * names[][2] = {
		{ "data/example" , "data_example" },
		{ "data/scene/select/character/01b_name/other" , "data_scene_select_character_01b_name_other" },
		{ "data/scene/title/with_underline" , "data_scene_title_with_underline" },
	};

	for (auto name : names) {
		package.appendFile(name[0], "test-data/empty");
	}

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE);

	int i = 0; for (auto& entry : package) {
		TS_ASSERT_EQUALS(strcmp(entry.getName(), names[i++][1]), 0);
	} TS_ASSERT_EQUALS(i, 3);

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH);

	i = 0; for (auto& entry : package) {
		TS_ASSERT_EQUALS(strcmp(entry.getName(), names[i++][0]), 0);
	} TS_ASSERT_EQUALS(i, 3);
}

void FilterSuite::testZipText() {
	ShadyCore::Package package;
	char buffer[15];
	package.appendPackage("test-data/zip-package.dat");

	buffer[0] = '\0';
	TS_ASSERT_EQUALS(package.findFile("data_my-text.cv0")->open().read(buffer, 15).gcount(), 12);
	package.findFile("data_my-text.cv0")->close();
	TS_ASSERT_EQUALS(strncmp(buffer, "Hello World!", 12), 0);

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION);

	buffer[0] = '\0';
	TS_ASSERT_EQUALS(package.findFile("data_my-text.txt")->open().read(buffer, 15).gcount(), 12);
	package.findFile("data_my-text.txt")->close();
	TS_ASSERT_EQUALS(strncmp(buffer, "Hello World!", 12), 0);

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION);

	buffer[0] = '\0';
	TS_ASSERT_EQUALS(package.findFile("data_my-text.cv0")->open().read(buffer, 15).gcount(), 12);
	package.findFile("data_my-text.cv0")->close();
	TS_ASSERT_EQUALS(strncmp(buffer, "Hello World!", 12), 0);
}

void FilterSuite::testDecrypt() {
	ShadyCore::Package package;
	package.appendPackage("test-data/encrypted");
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_DECRYPT_ALL);

	for (boost::filesystem::recursive_directory_iterator iter("test-data/decrypted"), e; iter != e; ++iter) {
		boost::filesystem::path fileName = boost::filesystem::relative(iter->path(), "test-data/decrypted").generic();

		if (boost::filesystem::is_regular_file(fileName)) {
			std::istream& input = package.findFile(fileName.string().c_str())->open();
			boost::filesystem::ifstream output(iter->path(), std::ios::binary);

			TS_ASSERT_STREAM(input, output);

			package.findFile(fileName.string().c_str())->close();
			output.close();
		}
	}
}

void FilterSuite::testEncrypt() {
	ShadyCore::Package package;
	package.appendPackage("test-data/decrypted");
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL);

	for (boost::filesystem::recursive_directory_iterator iter("test-data/encrypted"), e; iter != e; ++iter) {
		boost::filesystem::path fileName = boost::filesystem::relative(iter->path(), "test-data/encrypted").generic();

		if (boost::filesystem::is_regular_file(fileName)) {
			std::istream& input = package.findFile(fileName.string().c_str())->open();
			boost::filesystem::ifstream output(iter->path(), std::ios::binary);

			TS_ASSERT_STREAM(input, output);

			package.findFile(fileName.string().c_str())->close();
			output.close();
		}
	}
}
