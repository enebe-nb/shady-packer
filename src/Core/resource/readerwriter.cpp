#include "readerwriter.hpp"
#include <string>
#include <unordered_set>

namespace {
    using FT = ShadyCore::FileType;

    struct _comparator {
        using is_transparent = uint32_t;
        inline std::size_t operator()(const FT& v) const noexcept { return std::hash<uint32_t>()(v.extValue); }
        inline std::size_t operator()(const uint32_t v) const noexcept { return std::hash<uint32_t>()(v); }
        inline bool operator()(const FT& l, const FT& r) const noexcept { return l.extValue == r.extValue; }
        inline bool operator()(const uint32_t l, const FT& r) const noexcept { return l == r.extValue; }
    };

    const std::unordered_set<FT, _comparator, _comparator> typeMap = {
        FT(FT::TYPE_TEXT, FT::TEXT_NORMAL, FT::getExtValue(".txt")),
        FT(FT::TYPE_TEXT, FT::FORMAT_UNKNOWN, FT::getExtValue(".cv0")),
        FT(FT::TYPE_TABLE, FT::TABLE_CSV, FT::getExtValue(".csv")),
        FT(FT::TYPE_TABLE, FT::FORMAT_UNKNOWN, FT::getExtValue(".cv1")),
        FT(FT::TYPE_LABEL, FT::LABEL_LBL, FT::getExtValue(".lbl")),
        FT(FT::TYPE_LABEL, FT::LABEL_RIFF, FT::getExtValue(".sfl")),
        FT(FT::TYPE_IMAGE, FT::IMAGE_PNG, FT::getExtValue(".png")),
        FT(FT::TYPE_IMAGE, FT::IMAGE_GAME, FT::getExtValue(".cv2")),
        FT(FT::TYPE_PALETTE, FT::PALETTE_ACT, FT::getExtValue(".act")),
        FT(FT::TYPE_PALETTE, FT::PALETTE_PAL, FT::getExtValue(".pal")),
        FT(FT::TYPE_SFX, FT::SFX_WAV, FT::getExtValue(".wav")),
        FT(FT::TYPE_SFX, FT::SFX_GAME, FT::getExtValue(".cv3")),

        FT(FT::TYPE_SCHEMA, FT::SCHEMA_XML, FT::getExtValue(".xml")),
        FT(FT::TYPE_SCHEMA, FT::SCHEMA_GAME_GUI, FT::getExtValue(".dat")),
        FT(FT::TYPE_SCHEMA, FT::FORMAT_UNKNOWN, FT::getExtValue(".pat")),
    };
}

ShadyCore::FileType ShadyCore::FileType::get(uint32_t extValue) {
    auto i = typeMap.find(extValue);
    if (i == typeMap.end()) return FileType(extValue);
    return *i;
}

namespace _private {
    void readerText(ShadyCore::TextResource& resource, std::istream& input);
    void readerTextCv(ShadyCore::TextResource& resource, std::istream& input);
    void writerText(ShadyCore::TextResource& resource, std::ostream& output);
    void writerTextCv(ShadyCore::TextResource& resource, std::ostream& output);

    void readerLabel(ShadyCore::LabelResource& resource, std::istream& input);
    void readerLabelSfl(ShadyCore::LabelResource& resource, std::istream& input);
    void writerLabel(ShadyCore::LabelResource& resource, std::ostream& output);
    void writerLabelSfl(ShadyCore::LabelResource& resource, std::ostream& output);

    void readerImagePng(ShadyCore::Image& resource, std::istream& input);
    void readerImageCv(ShadyCore::Image& resource, std::istream& input);
    void writerImagePng(ShadyCore::Image& resource, std::ostream& output);
    void writerImageCv(ShadyCore::Image& resource, std::ostream& output);

    void readerPaletteAct(ShadyCore::Palette& resource, std::istream& input);
    void readerPalette(ShadyCore::Palette& resource, std::istream& input);
    void writerPaletteAct(ShadyCore::Palette& resource, std::ostream& output);
    void writerPalette(ShadyCore::Palette& resource, std::ostream& output);

    void readerSfxWave(ShadyCore::Sfx& resource, std::istream& input);
    void readerSfxCv(ShadyCore::Sfx& resource, std::istream& input);
    void writerSfxWave(ShadyCore::Sfx& resource, std::ostream& output);
    void writerSfxCv(ShadyCore::Sfx& resource, std::ostream& output);

    void readerSchemaXml(ShadyCore::Schema& resource, std::istream& input);
    void readerSchemaGui(ShadyCore::Schema& resource, std::istream& input);
    void readerSchemaAnim(ShadyCore::Schema& resource, std::istream& input);
    void readerSchemaMove(ShadyCore::Schema& resource, std::istream& input);
    void writerSchemaXml(ShadyCore::Schema& resource, std::ostream& output);
    void writerSchemaGui(ShadyCore::Schema& resource, std::ostream& output);
    void writerSchemaAnim(ShadyCore::Schema& resource, std::ostream& output);
    void writerSchemaMove(ShadyCore::Schema& resource, std::ostream& output);
}

static constexpr uint32_t _pack(ShadyCore::FileType::Type type, ShadyCore::FileType::Format format)
    { return (type << 16) | format; }

ShadyCore::ResourceReader_t ShadyCore::getResourceReader(const ShadyCore::FileType& type) {
    switch (_pack(type.type, type.format)) {
        case _pack(FileType::TYPE_TEXT, FileType::TEXT_GAME): return (ShadyCore::ResourceReader_t)_private::readerTextCv;
        case _pack(FileType::TYPE_TEXT, FileType::TEXT_NORMAL): return (ShadyCore::ResourceReader_t)_private::readerText;
        case _pack(FileType::TYPE_TABLE, FileType::TABLE_GAME): return (ShadyCore::ResourceReader_t)_private::readerTextCv;
        case _pack(FileType::TYPE_TABLE, FileType::TABLE_CSV): return (ShadyCore::ResourceReader_t)_private::readerText;
        case _pack(FileType::TYPE_LABEL, FileType::LABEL_RIFF): return (ShadyCore::ResourceReader_t)_private::readerLabelSfl;
        case _pack(FileType::TYPE_LABEL, FileType::LABEL_LBL): return (ShadyCore::ResourceReader_t)_private::readerLabel;
        case _pack(FileType::TYPE_IMAGE, FileType::IMAGE_GAME): return (ShadyCore::ResourceReader_t)_private::readerImageCv;
        case _pack(FileType::TYPE_IMAGE, FileType::IMAGE_PNG): return (ShadyCore::ResourceReader_t)_private::readerImagePng;
        case _pack(FileType::TYPE_PALETTE, FileType::PALETTE_PAL): return (ShadyCore::ResourceReader_t)_private::readerPalette;
        case _pack(FileType::TYPE_PALETTE, FileType::PALETTE_ACT): return (ShadyCore::ResourceReader_t)_private::readerPaletteAct;
        case _pack(FileType::TYPE_SFX, FileType::SFX_GAME): return (ShadyCore::ResourceReader_t)_private::readerSfxCv;
        case _pack(FileType::TYPE_SFX, FileType::SFX_WAV): return (ShadyCore::ResourceReader_t)_private::readerSfxWave;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_XML): return (ShadyCore::ResourceReader_t)_private::readerSchemaXml;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_GAME_GUI): return (ShadyCore::ResourceReader_t)_private::readerSchemaGui;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_GAME_ANIM): return (ShadyCore::ResourceReader_t)_private::readerSchemaAnim;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_GAME_PATTERN): return (ShadyCore::ResourceReader_t)_private::readerSchemaMove;

        case _pack(FileType::TYPE_BGM, FileType::BGM_OGG): //return (ShadyCore::ResourceReader_t)_private::readerText;
        default: throw std::runtime_error("Trying to get the reader of an unknown resource."); // TODO maybe use a messagebox
    }
}

ShadyCore::ResourceWriter_t ShadyCore::getResourceWriter(const ShadyCore::FileType& type) {
    switch (_pack(type.type, type.format)) {
        case _pack(FileType::TYPE_TEXT, FileType::TEXT_GAME): return (ShadyCore::ResourceWriter_t)_private::writerTextCv;
        case _pack(FileType::TYPE_TEXT, FileType::TEXT_NORMAL): return (ShadyCore::ResourceWriter_t)_private::writerText;
        case _pack(FileType::TYPE_TABLE, FileType::TABLE_GAME): return (ShadyCore::ResourceWriter_t)_private::writerTextCv;
        case _pack(FileType::TYPE_TABLE, FileType::TABLE_CSV): return (ShadyCore::ResourceWriter_t)_private::writerText;
        case _pack(FileType::TYPE_LABEL, FileType::LABEL_RIFF): return (ShadyCore::ResourceWriter_t)_private::writerLabelSfl;
        case _pack(FileType::TYPE_LABEL, FileType::LABEL_LBL): return (ShadyCore::ResourceWriter_t)_private::writerLabel;
        case _pack(FileType::TYPE_IMAGE, FileType::IMAGE_GAME): return (ShadyCore::ResourceWriter_t)_private::writerImageCv;
        case _pack(FileType::TYPE_IMAGE, FileType::IMAGE_PNG): return (ShadyCore::ResourceWriter_t)_private::writerImagePng;
        case _pack(FileType::TYPE_PALETTE, FileType::PALETTE_PAL): return (ShadyCore::ResourceWriter_t)_private::writerPalette;
        case _pack(FileType::TYPE_PALETTE, FileType::PALETTE_ACT): return (ShadyCore::ResourceWriter_t)_private::writerPaletteAct;
        case _pack(FileType::TYPE_SFX, FileType::SFX_GAME): return (ShadyCore::ResourceWriter_t)_private::writerSfxCv;
        case _pack(FileType::TYPE_SFX, FileType::SFX_WAV): return (ShadyCore::ResourceWriter_t)_private::writerSfxWave;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_XML): return (ShadyCore::ResourceWriter_t)_private::writerSchemaXml;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_GAME_GUI): return (ShadyCore::ResourceWriter_t)_private::writerSchemaGui;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_GAME_ANIM): return (ShadyCore::ResourceWriter_t)_private::writerSchemaAnim;
        case _pack(FileType::TYPE_SCHEMA, FileType::SCHEMA_GAME_PATTERN): return (ShadyCore::ResourceWriter_t)_private::writerSchemaMove;

        case _pack(FileType::TYPE_BGM, FileType::BGM_OGG): //return (ShadyCore::ResourceWriter_t)_private::readerText;
        default: throw std::runtime_error("Trying to get the writer of an unknown resource."); // TODO maybe use a messagebox
    }
}

ShadyCore::Resource* ShadyCore::createResource(const ShadyCore::FileType::Type type) {
    switch(type) {
        case FileType::TYPE_TEXT:
        case FileType::TYPE_TABLE: return new TextResource;
        case FileType::TYPE_LABEL: return new LabelResource;
        case FileType::TYPE_IMAGE: return new Image;
        case FileType::TYPE_PALETTE: return new Palette;
        case FileType::TYPE_SFX: return new Sfx;
        case FileType::TYPE_SCHEMA: return new Schema;
        default: throw std::runtime_error("Trying to create unknown resource."); // TODO maybe use a messagebox
    }
}

void ShadyCore::destroyResource(const ShadyCore::FileType::Type type, ShadyCore::Resource* resource) {
    switch(type) {
        case FileType::TYPE_TEXT:
        case FileType::TYPE_TABLE: reinterpret_cast<TextResource*>(resource)->destroy(); break;
        case FileType::TYPE_LABEL: break; // no destroy
        case FileType::TYPE_IMAGE: reinterpret_cast<Image*>(resource)->destroy(); break;
        case FileType::TYPE_PALETTE: reinterpret_cast<Palette*>(resource)->destroy(); break;
        case FileType::TYPE_SFX: reinterpret_cast<Sfx*>(resource)->destroy(); break;
        case FileType::TYPE_SCHEMA: reinterpret_cast<Schema*>(resource)->destroy(); break;
    } delete resource;
}

void ShadyCore::convertResource(const FileType::Type type, const FileType::Format inputFormat, std::istream& input, const FileType::Format outputFormat, std::ostream& output) {
    if (inputFormat == outputFormat) { output << input.rdbuf(); return; }

    Resource* resource = createResource(type);
    getResourceReader(FileType(type, inputFormat))(resource, input);
    getResourceWriter(FileType(type, outputFormat))(resource, output);
    destroyResource(type, resource);
}

