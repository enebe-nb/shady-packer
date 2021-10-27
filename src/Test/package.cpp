#include "../Core/package.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "util.hpp"
#include <fstream>
#include <gtest/gtest.h>
 
// TODO complete tests ???
/*
TEST(PackageSuite, StreamEntry) {
	ShadyCore::Package package;
	std::stringstream entryBase("Hello World!");
	ShadyCore::StreamPackageEntry entry(0, entryBase, "stream-entry", 12);
	char buffer[15];

	EXPECT_EQ(entry.open().read(buffer, 15).gcount(), 12);
	EXPECT_EQ(strncmp(buffer, "Hello World!", 12), 0);

	entry.close();
}

TEST(PackageSuite, ReplaceEntry) {
	ShadyCore::Package package;
	package.appendFile("data/my-text.txt", "test-data/decrypted/data/my-pattern.xml");
	package.appendFile("data/my-text.txt", "test-data/decrypted/data/my-text.txt");

	char buffer[13];
	EXPECT_EQ(package.findFile("data/my-text.txt")->open().read(buffer, 13).gcount(), 12);
	buffer[12] = '\0';
	EXPECT_EQ(strcmp(buffer, "Hello World!"), 0);
	package.findFile("data/my-text.txt")->close();

	package.appendPackage("test-data/data-package.dat");
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_SLASH_TO_UNDERLINE);
	package.appendPackage("test-data/zip-package.dat");

	EXPECT_EQ(package.findFile("data_my-text.cv0")->open().read(buffer, 13).gcount(), 12);
	buffer[12] = '\0';
	EXPECT_EQ(strcmp(buffer, "Hello World!"), 0);
	package.findFile("data_my-text.cv0")->close();
}
*/
TEST(PackageSuite, FileType) {
	ShadyCore::FileType type;

	type = ShadyCore::FileType::get("data/my-text.txt");
	EXPECT_EQ(type, ShadyCore::FileType::TYPE_TEXT);
	EXPECT_EQ(type.format, ShadyCore::FileType::TEXT_NORMAL);

	type = ShadyCore::FileType::get("data/my-gui.dat");
	EXPECT_EQ(type, ShadyCore::FileType::TYPE_SCHEMA);
	EXPECT_EQ(type.format, ShadyCore::FileType::SCHEMA_GAME_GUI);

	type = ShadyCore::FileType::get("data/my-unknown.typ");
	EXPECT_EQ(type, ShadyCore::FileType::TYPE_UNKNOWN);
	EXPECT_EQ(type.format, ShadyCore::FileType::FORMAT_UNKNOWN);

	ShadyCore::PackageEx package("test-data/encrypted");
	package.insert("data/my-effect.pat");
	package.insert("data/my-pattern.pat");
	ShadyCore::Package::iterator i;

	i = package.find("data/my-effect.pat");
	ASSERT_FALSE(i == package.end());
	type = i.fileType();
	EXPECT_EQ(type.type, ShadyCore::FileType::TYPE_SCHEMA);
	EXPECT_EQ(type.format, ShadyCore::FileType::SCHEMA_GAME_ANIM);

	i = package.find("data/my-pattern.pat");
	ASSERT_FALSE(i == package.end());
	type = i.fileType();
	EXPECT_EQ(type.type, ShadyCore::FileType::TYPE_SCHEMA);
	EXPECT_EQ(type.format, ShadyCore::FileType::SCHEMA_GAME_PATTERN);
}

// TODO Zip Convert
/*
TEST(FilterSuite, Underline) {
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
		EXPECT_TRUE(package.findFile(names[i  ][0]) == package.end());
		EXPECT_TRUE(package.findFile(names[i++][1]) != package.end());
	} EXPECT_EQ(i, 3);

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_UNDERLINE_TO_SLASH);

	i = 0; for (auto& entry : package) {
		EXPECT_TRUE(package.findFile(names[i  ][0]) != package.end());
		EXPECT_TRUE(package.findFile(names[i++][1]) == package.end());
	} EXPECT_EQ(i, 3);
}

TEST(FilterSuite, ZipText) {
	ShadyCore::Package package;
	char buffer[15];
	package.appendPackage("test-data/zip-package.dat");

	buffer[0] = '\0';
	EXPECT_EQ(package.findFile("data_my-text.cv0")->open().read(buffer, 15).gcount(), 12);
	package.findFile("data_my-text.cv0")->close();
	EXPECT_EQ(strncmp(buffer, "Hello World!", 12), 0);

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_FROM_ZIP_TEXT_EXTENSION);

	buffer[0] = '\0';
	EXPECT_EQ(package.findFile("data_my-text.txt")->open().read(buffer, 15).gcount(), 12);
	package.findFile("data_my-text.txt")->close();
	EXPECT_EQ(strncmp(buffer, "Hello World!", 12), 0);

	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_TO_ZIP_TEXT_EXTENSION);

	buffer[0] = '\0';
	EXPECT_EQ(package.findFile("data_my-text.cv0")->open().read(buffer, 15).gcount(), 12);
	package.findFile("data_my-text.cv0")->close();
	EXPECT_EQ(strncmp(buffer, "Hello World!", 12), 0);
}

TEST(FilterSuite, Decrypt) {
	ShadyCore::Package package;
	package.appendPackage("test-data/encrypted");
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_DECRYPT_ALL);

	for (std::filesystem::recursive_directory_iterator iter("test-data/decrypted"), e; iter != e; ++iter) {
		std::filesystem::path fileName = std::filesystem::relative(iter->path(), "test-data/decrypted").generic_string();

		if (std::filesystem::is_regular_file(fileName)) {
			std::istream& input = package.findFile(fileName.string().c_str())->open();
			std::ifstream output(iter->path(), std::ios::binary);

			ASSERT_STREAM(input, output);

			package.findFile(fileName.string().c_str())->close();
			output.close();
		}
	}
}

TEST(FilterSuite, Encrypt) {
	ShadyCore::Package package;
	package.appendPackage("test-data/decrypted");
	ShadyCore::PackageFilter::apply(package, ShadyCore::PackageFilter::FILTER_ENCRYPT_ALL);

	for (std::filesystem::recursive_directory_iterator iter("test-data/encrypted"), e; iter != e; ++iter) {
		std::filesystem::path fileName = std::filesystem::relative(iter->path(), "test-data/encrypted").generic_string();

		if (std::filesystem::is_regular_file(fileName)) {
			std::istream& input = package.findFile(fileName.string().c_str())->open();
			std::ifstream output(iter->path(), std::ios::binary);

			ASSERT_STREAM(input, output);

			package.findFile(fileName.string().c_str())->close();
			output.close();
		}
	}
}
*/
