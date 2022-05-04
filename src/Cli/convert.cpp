#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"
#include <fstream>

void ShadyCli::ConvertCommand::buildOptions() {
    options.add_options()
        ("c,copy-unknown", "Copy files that can't be converted")
        ("o,output", "Target directory to save", cxxopts::value<std::string>()->default_value(std::filesystem::current_path().string()))
        ("files", "List of files to convert", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

namespace {
    using FT = ShadyCore::FileType;

    const std::unordered_map<uint32_t, std::pair<FT, FT>> typeMap = {
        {FT::getExtValue(".txt"), {FT(FT::TYPE_TEXT, FT::TEXT_NORMAL, FT::getExtValue(".txt")), FT(FT::TYPE_TEXT, FT::TEXT_GAME, FT::getExtValue(".cv0"))}},
        {FT::getExtValue(".cv0"), {FT(FT::TYPE_TEXT, FT::TEXT_GAME, FT::getExtValue(".cv0")), FT(FT::TYPE_TEXT, FT::TEXT_NORMAL, FT::getExtValue(".txt"))}},
        {FT::getExtValue(".csv"), {FT(FT::TYPE_TABLE, FT::TABLE_CSV, FT::getExtValue(".csv")), FT(FT::TYPE_TABLE, FT::TABLE_GAME, FT::getExtValue(".cv1"))}},
        {FT::getExtValue(".cv1"), {FT(FT::TYPE_TABLE, FT::TABLE_GAME, FT::getExtValue(".cv1")), FT(FT::TYPE_TABLE, FT::TABLE_CSV, FT::getExtValue(".csv"))}},
        {FT::getExtValue(".lbl"), {FT(FT::TYPE_LABEL, FT::LABEL_LBL, FT::getExtValue(".lbl")), FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl"))}},
        {FT::getExtValue(".sfl"), {FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl")), FT(FT::TYPE_LABEL, FT::LABEL_LBL, FT::getExtValue(".lbl"))}},
        {FT::getExtValue(".bmp"), {FT(FT::TYPE_IMAGE, FT::IMAGE_BMP, FT::getExtValue(".bmp")), FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2"))}},
        {FT::getExtValue(".png"), {FT(FT::TYPE_IMAGE, FT::IMAGE_PNG, FT::getExtValue(".png")), FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2"))}},
        {FT::getExtValue(".cv2"), {FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2")), FT(FT::TYPE_IMAGE, FT::IMAGE_PNG, FT::getExtValue(".png"))}},
        {FT::getExtValue(".act"), {FT(FT::TYPE_PALETTE, FT::PALETTE_ACT, FT::getExtValue(".act")), FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal"))}},
        {FT::getExtValue(".pal"), {FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal")), FT(FT::TYPE_PALETTE, FT::PALETTE_ACT, FT::getExtValue(".act"))}},
        {FT::getExtValue(".wav"), {FT(FT::TYPE_SFX, FT::SFX_WAV, FT::getExtValue(".wav")), FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3"))}},
        {FT::getExtValue(".cv3"), {FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3")), FT(FT::TYPE_SFX, FT::SFX_WAV, FT::getExtValue(".wav"))}},

        {FT::getExtValue(".xml"), {FT(FT::TYPE_SCHEMA, FT::SCHEMA_XML, FT::getExtValue(".xml")), FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN, FT::getExtValue(".pat"))}},
        {FT::getExtValue(".dat"), {FT(FT::TYPE_SCHEMA, FT::SCHEMA_GAME_GUI, FT::getExtValue(".dat")), FT(FT::TYPE_SCHEMA, FT::SCHEMA_XML, FT::getExtValue(".xml"))}},
        {FT::getExtValue(".pat"), {FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN, FT::getExtValue(".pat")), FT(FT::TYPE_SCHEMA, FT::SCHEMA_XML, FT::getExtValue(".xml"))}},
    };
}

static inline FT::Format findXmlFormat(const FT& inputType, std::istream& input) {
    char buffer[16];
    while (input.get(buffer[0]) && input.gcount()) if (buffer[0] == '<') {
        int j = 0;
        for (input.get(buffer[0]); j < 16 && buffer[j] && !strchr(" />", buffer[j]); input.get(buffer[++j]));
        buffer[j] = '\0';
        if (strcmp(buffer, "movepattern") == 0) return FT::SCHEMA_GAME_PATTERN;
        if (strcmp(buffer, "animpattern") == 0) return FT::SCHEMA_GAME_ANIM;
        if (strcmp(buffer, "layout") == 0) return FT::SCHEMA_GAME_GUI;
        if (!strchr("?", buffer[0])) break;
    }
}

void ShadyCli::ConvertCommand::processFile(std::filesystem::path filename, std::filesystem::path targetPath, bool copyUnknown) {
    auto iter = typeMap.find(FT::getExtValue(filename.extension().generic_string().c_str()));
    if (iter == typeMap.end()) {
        if (copyUnknown) {
            printf("Copying %s -> %s\n", filename.generic_string().c_str(), (targetPath / filename.filename()).generic_string().c_str());
            std::filesystem::create_directories(targetPath);
            std::filesystem::copy_file(filename, targetPath / filename.filename());
        }
    } else {
        auto type = iter->second;
        std::ifstream input(filename, std::fstream::binary);
        if (type.first.type == FT::SCHEMA_XML) {
            type.second.format = findXmlFormat(type.first, input);
            if (type.second.format == FT::SCHEMA_GAME_GUI) type.second.extValue = FT::getExtValue(".dat");
            input.seekg(0);
        }
        std::filesystem::path targetName = targetPath / filename.filename(); type.second.appendExtValue(targetName);
        printf("Converting %s -> %s\n", filename.generic_string().c_str(), targetName.generic_string().c_str());
        std::filesystem::create_directories(targetPath);
        std::ofstream output(targetName.string(), std::fstream::binary);
        ShadyCore::convertResource(type.first.type, type.first.format, input, type.second.format, output);
    }
}

bool ShadyCli::ConvertCommand::run(const cxxopts::ParseResult& result) {
    std::filesystem::path outputPath(result["output"].as<std::string>());
    bool copyUnknown = result.count("copy-unknown");
    auto& files = result["files"].as<std::vector<std::string> >();

    size_t size = files.size();
    for (size_t i = 0; i < size; ++i) {
        std::filesystem::path path(files[i]);
        if (std::filesystem::is_directory(path)) {
            for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!std::filesystem::is_directory(iter->path())) {
                    processFile(iter->path(), outputPath / std::filesystem::relative(iter->path().parent_path(), path), copyUnknown);
                }
            }
        } else if (std::filesystem::exists(path)) {
            processFile(path, outputPath, copyUnknown);
        }
    }

    return true;
}
