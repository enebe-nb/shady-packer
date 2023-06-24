#include "../Core/package.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "../Core/util/tempfiles.hpp"
#include "util.hpp"
#include <fstream>
#include <gtest/gtest.h>
 
TEST(PackageSuite, DontReplaceEntry) {
	ShadyCore::PackageEx package;
	package.insert("data/my-text.txt", "test-data/decrypted/data/my-text.txt");
	package.insert("data/my-text.txt", "test-data/decrypted/data/my-pattern.xml");

	char buffer[13];
	auto* input = &package.find("data/my-text.txt").open();
	EXPECT_EQ(input->read(buffer, 13).gcount(), 12);
	buffer[12] = '\0';
	EXPECT_EQ(strcmp(buffer, "Hello World!"), 0);
	package.find("data/my-text.txt").close(*input);

	package.merge("test-data/zip-package.dat");
	package.merge("test-data/data-package.dat");

	auto iter = package.find("data/my-text.cv0");
	input = &iter.open();
	EXPECT_EQ(iter.fileType().format, ShadyCore::FileType::TEXT_NORMAL);
	EXPECT_EQ(input->read(buffer, 13).gcount(), 12);
	buffer[12] = '\0';
	EXPECT_EQ(strcmp(buffer, "Hello World!"), 0);
	iter.close(*input);
}

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

// TODO Underline convertions
