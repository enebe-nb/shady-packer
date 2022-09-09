#include "main.hpp"

#include <sstream>
#include <SokuLib.hpp>
#include <curl/curl.h>
#include <memory>

#include "../Core/package.hpp"
#include "../Core/reader.hpp"
#include "modpackage.hpp"
#include "menu.hpp"
#include "../Lua/lualibs/soku.hpp"

void* ShadyCore::Allocate(size_t s) { return SokuLib::NewFct(s); }
void ShadyCore::Deallocate(void* p) { SokuLib::DeleteFct(p); }

namespace {
    auto __textReader = reinterpret_cast<bool (__fastcall *)(int output, const char* filename)>(0x00408a20);
    auto __tableReader = reinterpret_cast<bool (__fastcall *)(int output, const char* filename)>(0x00408ab0);
    auto __labelReader = reinterpret_cast<bool (__fastcall *)(const char* filename, int unused, int output)>(0x00418eb0);
    auto __sfxReader = reinterpret_cast<bool (__fastcall *)(int output, const char* filename)>(0x00408b40);
    auto __paletteReaderA = reinterpret_cast<bool (__fastcall *)(int a, int unused, const char* filename, int output, int b)>(0x00408be0);
    auto __paletteReaderB = reinterpret_cast<bool (__fastcall *)(int a, int unused, const char* filename, int output, int b)>(0x00408be0);
    auto __bgmCreateReader = reinterpret_cast<bool (__fastcall *)(void** output, int unused, const char* filename)>(0x0040d1e0);
    auto __textureCreateReader = reinterpret_cast<bool (__fastcall *)(void** output, int unused, const char* filename)>(0x0040d1e0);
    auto __schemaCreateReaderA = reinterpret_cast<bool (__fastcall *)(void** output, int unused, const char* filename)>(0x0040d1e0);
    auto __schemaCreateReaderB = reinterpret_cast<bool (__fastcall *)(void** output, int unused, const char* filename)>(0x0040d1e0);
    auto __schemaCreateReaderC = reinterpret_cast<bool (__fastcall *)(void** output, int unused, const char* filename)>(0x0040d1e0);

    // avail code [0x408e30:0x408ea5] 
    // keep ebx state, return to 0x408ea5
    uint8_t __imageReader[] = {
        0x8D, 0x4C, 0x24, 0x2C,                     // lea ecx, [esp+2c](&BitmapData)
        0x8D, 0x94, 0x24, 0x8C, 0x00, 0x00, 0x00,   // lea edx, [esp+8c](filename[])
        0x53,                                       // push ebx
        0x8D, 0x84, 0x24, 0x98, 0x01, 0x00, 0x00,   // lea eax, [esp+198](tempBuffer[0x104])
        0x68, 0x04, 0x01, 0x00, 0x00,               // push 0x104
        0x50,                                       // push eax
        0xE8, 0x00, 0x00, 0x00, 0x00,               // call imageReader
        0x5B,                                       // pop ebx
        0xE9, 0x51, 0x00, 0x00, 0x00,               // jmp to return handling code
        0x90,                                       // NOP to align
    };

    //auto __readerCreate = reinterpret_cast<void* (__stdcall *)(const char *filename, unsigned int *size, unsigned int *offset)>(0x0041c080);
    auto __loader = reinterpret_cast<int(*)()>(0);
    bool _initialized = false;

    int (SokuLib::Title::* __titleOnProcess)();
    int (SokuLib::Title::* __titleOnRender)();

    struct {
        SokuLib::Sprite* sprite = 0;
        int timeout = 240;
        bool shown = false;
    } menuHint;

    const SokuLib::FontDescription menuHintFont {
        "Courier New",
        255,255,255,255,255,255,
        14, 500,
        false, true, false,
        0, 0, 0, 0, 0
    };
}

static bool __fastcall textReader(int output, const char* filename) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_TEXT);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            ShadyCore::getResourceReader(filetype)((ShadyCore::Resource*)output, iter.open());
            iter.close();
            return true;
        }
    } return __textReader(output, filename);
}

static bool __fastcall tableReader(int output, const char* filename) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_TABLE);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            ShadyCore::getResourceReader(filetype)((ShadyCore::Resource*)output, iter.open());
            iter.close();
            return true;
        }
    } return __tableReader(output, filename);
}

static bool __fastcall labelReader(const char* filename, int unused, int output) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_LABEL);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            ShadyCore::getResourceReader(filetype)((ShadyCore::Resource*)(output + 0x12e8), iter.open());
            iter.close();
            return true;
        }
    } return __labelReader(filename, unused, output);
}

static bool __fastcall sfxReader(int output, const char* filename) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_SFX);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            ShadyCore::getResourceReader(filetype)((ShadyCore::Resource*)output, iter.open());
            iter.close();
            return true;
        }
    } return __sfxReader(output, filename);
}

template <bool (__fastcall*& super)(int, int, const char*, int, int)>
static bool __fastcall paletteReader(int a, int unused, const char* filename, int output, int b) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_PALETTE);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            ShadyCore::getResourceReader(filetype)((ShadyCore::Resource*)output, iter.open());
            iter.close();
            return true;
        }
    } return super(a, unused, filename, output, b);
}

static bool __fastcall bgmCreateReader(void** output, int unused, const char* filename) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_BGM);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            int* reader = SokuLib::New<int>(6);
            reader[0] = ShadyCore::entry_reader_vtbl;
            reader[1] = (int)new ShadyCore::EntryReader(iter.entry());
            reader[3] = iter.entry().getSize();
            reader[2] = reader[4] = reader[5] = 0;
            *output = reader;
            return true;
        }
    } return __bgmCreateReader(output, unused, filename);
}

static bool __fastcall textureCreateReader(void** output, int unused, const char* filename) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_TEXTURE);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            int* reader = SokuLib::New<int>(6);
            reader[0] = ShadyCore::entry_reader_vtbl;
            reader[1] = (int)new ShadyCore::EntryReader(iter.entry());
            reader[3] = iter.entry().getSize();
            reader[2] = reader[4] = reader[5] = 0;
            *output = reader;
            return true;
        }
    } return __textureCreateReader(output, unused, filename);
}

static bool __fastcall imageReader(ShadyCore::Image* output, const char* filename, char* tempbuffer, size_t bufferSize) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_IMAGE);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            ShadyCore::getResourceReader(filetype)((ShadyCore::Resource*)output, iter.open());
            iter.close();
            return true;
        }
    }

    bufferSize -= 4;
    for (int i = 0; i < bufferSize; ++i) {
        tempbuffer[i] = filename[i];
        if (filename[i] == '.') {
            *(int*)&tempbuffer[i+1] = *(int*)"cv2";
            break;
        } else if(filename[i] == '\0') break;
    }

    return reinterpret_cast<bool(__fastcall*)(ShadyCore::Image*, int*, const char*)>(output->vtable[3])(output, 0, tempbuffer);
}

/* Kept for reference
static bool __fastcall guiReader(SokuLib::CDesign* output, int unused, const char* filename) {
    ModPackage::CheckUpdates();
    ShadyCore::Schema* schema = 0;
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_SCHEMA);
        if (iter != ModPackage::basePackage->end()) {
            auto filetype = iter.fileType();
            schema = new ShadyCore::Schema;
            ShadyCore::getResourceReader(filetype)(schema, iter.open());
            iter.close();
        }
    } if (!schema) return __guiReader(output, unused, filename);

    size_t offset = output->textures.size();
    std::string_view folder(filename);
    folder.remove_suffix(folder.size() - folder.rfind('/') - 1);
    output->textures.resize(offset + schema->images.size());
    for (int i = 0; i < schema->images.size(); ++i) {
        std::string imageFile(folder);
        imageFile += schema->images[i].name;
        SokuLib::textureMgr.loadTexture(&output->textures[offset + i], imageFile.c_str(), 0, 0);
    }

    for (int i = 0; i < schema->objects.size(); ++i) {
        auto object = schema->objects[i];
        switch(object->getType()) {
        case 0: {
            const auto in = reinterpret_cast<ShadyCore::Schema::GuiObject*>(object);
            const auto out = new SokuLib::CDesign::Sprite();
            const auto& texData = schema->images[in->imageIndex];
            out->x2 = in->x; out->y2 = in->y;
            out->sprite.setTexture(output->textures[offset + in->imageIndex], texData.x, texData.y, texData.w, texData.h, -texData.x, -texData.y);
            if (in->mirror) out->sprite.scale.x = -1;
            if (in->getId()) output->objectMap[in->getId()] = out;
            output->objects.push_back(out);
        } break;
        case 1: {
            const auto in = reinterpret_cast<ShadyCore::Schema::GuiObject*>(object);
            const auto out = new SokuLib::CDesign::Object();
            out->x2 = in->x; out->y2 = in->y;
            if (in->getId()) output->objectMap[in->getId()] = out;
            output->objects.push_back(out);
        } break;
        case 2: case 3: case 4: case 5: {
            const auto in = reinterpret_cast<ShadyCore::Schema::GuiObject*>(object);
            const auto out = new SokuLib::CDesign::Gauge();
            const auto& texData = schema->images[in->imageIndex];
            out->x2 = in->x; out->y2 = in->y;
            out->gauge.fromTexture(output->textures[offset + in->imageIndex], texData.w, texData.h, object->getType() - 2);
            if (in->mirror) out->gauge.scale.x = -1;
            if (in->getId()) output->objectMap[in->getId()] = out;
            output->objects.push_back(out);
        } break;
        case 6: {
            const auto in = reinterpret_cast<ShadyCore::Schema::GuiNumber*>(object);
            const auto out = new SokuLib::CDesign::Number();
            const auto& texData = schema->images[in->imageIndex];
            out->x2 = in->x; out->y2 = in->y;
            out->number.width = in->w; out->number.textSpacing = in->textSpacing;
            out->number.unknown0C = out->number.unknown10 = 1.f;
            out->number.fontSpacing = in->fontSpacing;
            out->number.size = in->size;
            out->number.floatSize = in->floatSize;
            out->number.unknown1C = false;
            out->number.tiles.createSlices(output->textures[offset + in->imageIndex], 0, 0, in->w, in->h, 0, 0);
            if (in->getId()) output->objectMap[in->getId()] = out;
            output->objects.push_back(out);
        } break;
        }
    }

    // this is an iterator, but there's difference between debug and release
    output->unknown0x2C = &output->objects;
    output->unknown0x30 = (void*) ((int**)&output->objects)[1][1];

    schema->destroy(); delete schema;
    return true;
}
*/

template <bool (__fastcall*& super)(void**, int, const char*), ShadyCore::FileType::Format targetFormat>
static bool __fastcall schemaCreateReader(void** output, int unused, const char* filename) {
    ModPackage::CheckUpdates();
    { std::shared_lock lock(*ModPackage::basePackage);
        auto iter = ModPackage::basePackage->find(filename, ShadyCore::FileType::TYPE_SCHEMA);
        if (iter != ModPackage::basePackage->end()) {
            auto type = iter.fileType();
            int* reader = (int*&)*output = SokuLib::New<int>(6);
            if (type.format != ShadyCore::FileType::SCHEMA_XML) {
                reader[0] = ShadyCore::entry_reader_vtbl;
                reader[1] = (int)new ShadyCore::EntryReader(iter.entry());
                reader[3] = iter.entry().getSize();
                reader[2] = reader[4] = reader[5] = 0;
            } else {
                // TODO schema loading is still too complex
                std::istream& input = iter.open();
                std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
                ShadyCore::convertResource(type.type, type.format, input, targetFormat, *buffer);
                iter.close();

                reader[0] = ShadyCore::stream_reader_vtbl;
                reader[1] = (int)buffer;
                reader[3] = buffer->tellp();
                reader[2] = reader[4] = reader[5] = 0;
            } return true;
        }
    } return super(output, unused, filename);
}

static int __fastcall titleOnProcess(SokuLib::Title* t) {
    int ret = (t->*__titleOnProcess)();

    if (!menuHint.sprite && !menuHint.shown) {
        menuHint.shown = true;

        int texture = 0;
        SokuLib::SWRFont font; font.create();
        font.setIndirect(menuHintFont);
        SokuLib::textureMgr.createTextTexture(&texture, "Enabled shady-loader. Press F2 to open the menu.", font, 640, 20, 0, 0);
        font.destruct();

        menuHint.sprite = new SokuLib::Sprite();
        menuHint.sprite->setTexture2(texture, 0, 0, 640, 20);
    } else if (menuHint.sprite && menuHint.timeout <= 0) {
        SokuLib::textureMgr.remove(menuHint.sprite->dxHandle);
        delete menuHint.sprite;
        menuHint.sprite = 0;
    }

    if (SokuLib::checkKeyOneshot(0x3C, false, false, false) && !*((int*)0x89a88c)) {
        // the virtual destructor will call free from this binary
        // so here should be `new` instead of `SokuLib::New`
        SokuLib::activateMenu(new ModMenu());
    }

    return ret;
}

static int __fastcall titleOnRender(SokuLib::Title* t) {
    int ret = (t->*__titleOnRender)();

    if (menuHint.sprite && menuHint.timeout > 0) {
        if (menuHint.timeout < 20) menuHint.sprite->setColor(((0xff*menuHint.timeout/20) << 24)|0xffffff);
        menuHint.sprite->render(0, 0);
        --menuHint.timeout;
    }

    return ret;
}

template<int N> static inline void TamperCode(int addr, uint8_t(&code)[N]) {
    for (int i = 0; i < N; ++i) {
        uint8_t swap = *(uint8_t*)(addr+i);
        *(uint8_t*)(addr+i) = code[i];
        code[i] = swap;
    }
}

static int _HookLoader() {
    if (__loader) __loader();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    ShadyCore::Palette::setTrueColorAvailable(GetModuleHandleA("true-color-palettes.dll"));

    DWORD dwOldProtect;
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    __textReader = SokuLib::TamperNearJmpOpr(0x00405853, textReader);
    __tableReader = SokuLib::TamperNearJmpOpr(0x0040f3b3, tableReader);
    __labelReader = SokuLib::TamperNearJmpOpr(0x00418cc5, labelReader);
    __sfxReader = SokuLib::TamperNearJmpOpr(0x0041869f, sfxReader);
    __paletteReaderA = SokuLib::TamperNearJmpOpr(0x00467b7d, paletteReader<__paletteReaderA>); // patternLoad
    __paletteReaderB = SokuLib::TamperNearJmpOpr(0x0041ffb7, paletteReader<__paletteReaderB>); // charSelect
    __bgmCreateReader = SokuLib::TamperNearJmpOpr(0x00418be1, bgmCreateReader);
    __textureCreateReader = SokuLib::TamperNearJmpOpr(0x00409196, textureCreateReader);
    TamperCode(0x00408e30, __imageReader);
            SokuLib::TamperNearJmpOpr(0x00408e49, imageReader);
    __schemaCreateReaderA = SokuLib::TamperNearJmpOpr(0x0040b36b, schemaCreateReader<__schemaCreateReaderA, ShadyCore::FileType::SCHEMA_GAME_GUI>);
    __schemaCreateReaderB = SokuLib::TamperNearJmpOpr(0x0043b4bf, schemaCreateReader<__schemaCreateReaderB, ShadyCore::FileType::SCHEMA_GAME_ANIM>);
    __schemaCreateReaderC = SokuLib::TamperNearJmpOpr(0x00467a80, schemaCreateReader<__schemaCreateReaderC, ShadyCore::FileType::SCHEMA_GAME_PATTERN>);
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    __titleOnProcess  = SokuLib::TamperDword(&SokuLib::VTable_Title.onProcess, titleOnProcess);
    __titleOnRender  = SokuLib::TamperDword(&SokuLib::VTable_Title.onRender, titleOnRender);
    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    _initialized = true;
    // set EAX to restore hooked instruction
    return *(int*)0x008943b8;
}

void HookLoader() {
    if (iniEnableLua) ShadyLua::LoadTamper();

    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    bool hasCall = 0xE8 == *(unsigned char*)0x007fb596;
    __loader = SokuLib::TamperNearCall(0x007fb596, _HookLoader);
    if (!hasCall) __loader = 0;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
}

void UnhookLoader() {
    ShadyLua::UnloadTamper();

    if (!_initialized) return;
    DWORD dwOldProtect;
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    SokuLib::TamperNearJmpOpr(0x00405853, __textReader);
    SokuLib::TamperNearJmpOpr(0x0040f3b3, __tableReader);
    SokuLib::TamperNearJmpOpr(0x00418cc5, __labelReader);
    SokuLib::TamperNearJmpOpr(0x0041869f, __sfxReader);
    SokuLib::TamperNearJmpOpr(0x00467b7d, __paletteReaderA); // patternLoad
    SokuLib::TamperNearJmpOpr(0x0041ffb7, __paletteReaderB); // charSelect
    SokuLib::TamperNearJmpOpr(0x00418be1, __bgmCreateReader);
    SokuLib::TamperNearJmpOpr(0x00409196, __textureCreateReader);
    TamperCode(0x00408e30, __imageReader);
    SokuLib::TamperNearJmpOpr(0x0040b36b, __schemaCreateReaderA); // gui
    SokuLib::TamperNearJmpOpr(0x0043b4bf, __schemaCreateReaderB); // effect
    SokuLib::TamperNearJmpOpr(0x00467a80, __schemaCreateReaderC); // pattern
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    SokuLib::TamperDword(&SokuLib::VTable_Title.onProcess, __titleOnProcess);
    SokuLib::TamperDword(&SokuLib::VTable_Title.onRender, __titleOnRender);
    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    // TODO join downloadController before this
    curl_global_cleanup();
}