#include "../Core/package.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "../Core/util/tempfiles.hpp"
#include "util.hpp"
#include <fstream>
#include <gtest/gtest.h>
#include <benchmark/benchmark.h>

using FT = ShadyCore::FileType;
struct ConvertionData {
	FT::Type type;
	const std::vector<std::pair<FT::Format, const std::filesystem::path> > files;
};

class ReaderWriterSuite : public ::testing::TestWithParam<ConvertionData> {
public:
	const static ConvertionData dataArray[];
};

const ConvertionData ReaderWriterSuite::dataArray[] = {
	{ FT::TYPE_TEXT, {
		{FT::TEXT_GAME, "test-data/all/my-text.cv0"},
		{FT::TEXT_NORMAL, "test-data/all/my-text.txt"},
	}},
	{ FT::TYPE_TABLE, {
		{FT::TABLE_GAME, "test-data/all/my-text.cv1"},
		{FT::TABLE_CSV, "test-data/all/my-text.csv"},
	}},
	{ FT::TYPE_LABEL, {
		{FT::LABEL_RIFF, "test-data/all/my-label.sfl"},
		{FT::LABEL_LBL, "test-data/all/my-label.lbl"},
	}},
	{ FT::TYPE_IMAGE, {
		{FT::IMAGE_GAME, "test-data/all/my-image.cv2"},
		{FT::IMAGE_PNG, "test-data/all/my-image.png"},
		{FT::IMAGE_BMP, "test-data/all/my-image.bmp"},
	}},
	{ FT::TYPE_IMAGE, {
		{FT::IMAGE_GAME, "test-data/all/my-image-indexed.cv2"},
		{FT::IMAGE_PNG, "test-data/all/my-image-indexed.png"},
	}},
	{ FT::TYPE_PALETTE, {
		{FT::PALETTE_PAL, "test-data/all/my-palette.pal"},
		{FT::PALETTE_ACT, "test-data/all/my-palette.act"},
	}},
	{ FT::TYPE_SFX, {
		{FT::SFX_GAME, "test-data/all/my-sfx.cv3"},
		{FT::SFX_WAV, "test-data/all/my-sfx.wav"},
	}},
	{ FT::TYPE_SCHEMA, {
		{FT::SCHEMA_GAME_GUI, "test-data/all/my-gui.dat"},
		{FT::SCHEMA_XML, "test-data/all/my-gui.xml"},
	}},
	{ FT::TYPE_SCHEMA, {
		{FT::SCHEMA_GAME_PATTERN, "test-data/all/my-pattern.pat"},
		{FT::SCHEMA_XML, "test-data/all/my-pattern.xml"},
	}},
	{ FT::TYPE_SCHEMA, {
		{FT::SCHEMA_GAME_ANIM, "test-data/all/my-effect.pat"},
		{FT::SCHEMA_XML, "test-data/all/my-effect.xml"},
	}},
};

TEST_P(ReaderWriterSuite, Convertion) {
	auto data = GetParam();
	for (auto& inputData : data.files){
		std::ifstream input(inputData.second, std::ios::binary);
		for (auto& outputData : data.files) {
			std::stringstream output;
			ShadyCore::convertResource(data.type, inputData.first, input, outputData.first, output);
			std::ifstream expected(outputData.second, std::ios::binary);
			EXPECT_TRUE(testing::isSameData(output, expected)) << "in: " << inputData.second << ", out: " << outputData.second;
			input.seekg(0);
		}
	}
}

TEST_P(ReaderWriterSuite, PackageRead) {
	auto data = GetParam();

	std::filesystem::path zipFile = ShadyUtil::TempFile();
	std::filesystem::path dataFile = ShadyUtil::TempFile();
	{
		ShadyCore::PackageEx package;
		package.insert(data.files[0].second.filename().string(), data.files[0].second);
		package.save(zipFile, ShadyCore::Package::ZIP_MODE);
		package.save(dataFile, ShadyCore::Package::DATA_MODE);
		package.clear();
	}

	for (auto file : { zipFile, dataFile }) {
		ShadyCore::Resource* resource;
		ShadyCore::Package package(file);
		auto iter = package.find(data.files[0].second.filename().string());
		ASSERT_TRUE(iter != package.end()) << "find(" << data.files[0].second.filename().string() << ")";
		for (int i = 1; i < data.files.size(); ++i) {
			ASSERT_EQ(iter, package.find(data.files[i].second.filename().string()))
				<< "equal(" << data.files[i].second.filename().string() << ")";
		}

		ShadyCore::FileType type = iter.fileType();
		std::istream &input = iter.open();
		for (int i = 0; i < data.files.size(); ++i) {
			std::stringstream output;
			ShadyCore::convertResource(type.type, type.format, input, data.files[i].first, output);
			std::ifstream expected(data.files[i].second, std::ios::binary);
			EXPECT_TRUE(testing::isSameData(output, expected))  << "package["<< data.files[i].second.filename() << "]";
			input.seekg(0);
		} iter.close();
	}

	std::filesystem::remove(zipFile);
	std::filesystem::remove(dataFile);
}

INSTANTIATE_TEST_SUITE_P(FileList, ReaderWriterSuite, ::testing::ValuesIn(ReaderWriterSuite::dataArray) );
