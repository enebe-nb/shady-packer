#include "../lualibs.hpp"
#include "../script.hpp"
#include "../../Core/resource.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/RefCountedPtr.h>
#include <sstream>
#include <fstream>
#include <cmath>

using namespace luabridge;

namespace {
    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}

    class ResourceProxy {
    public:
        ShadyCore::Resource* const resource;
        const ShadyCore::FileType::Type type;
        const bool isOwner;

        inline ResourceProxy(ShadyCore::Resource* resource, ShadyCore::FileType::Type type, bool isOwner = true)
            : resource(resource), type(type), isOwner(isOwner) {}
        template <class T> ResourceProxy(T* resource, bool isOwner = true);
        inline ~ResourceProxy() { if(isOwner) ShadyCore::destroyResource(type, resource); }
    };
}

/** Creates a resource from a file in script space */
static RefCountedPtr<ResourceProxy> resource_createfromfile(const char* filename, int format, lua_State* L) {
    ShadyLua::LuaScript* script = ShadyLua::ScriptMap.at(L);

    std::stringstream data;
    char buffer[4096]; size_t size;
    void* file = script->open(filename);
    while (size = script->read(file, buffer, 4096)) {
        data.write(buffer, size);
    }

    auto type = ShadyCore::FileType::get(filename).type;
    RefCountedPtr<ResourceProxy> proxy(new ResourceProxy(ShadyCore::createResource(type), type, true));
    ShadyCore::getResourceReader(ShadyCore::FileType(type, (ShadyCore::FileType::Format)format))(proxy->resource, data);
    return proxy;
}

/** Saves a resource into filesystem */
static void resource_export(RefCountedPtr<ResourceProxy> resource, const char* filename, int format, lua_State* L) {
    std::fstream out(filename, std::ios::out|std::ios::binary);
    ShadyCore::getResourceWriter(ShadyCore::FileType(resource->type, (ShadyCore::FileType::Format)format))
        (resource->resource, out);
}

static int resource_Text_getData(lua_State* L) {
    ShadyCore::TextResource* resource = Stack<ShadyCore::TextResource*>::get(L, 1);
    if (!resource->data) lua_pushstring(L, "");
    else lua_pushlstring(L, resource->data, resource->length);
    return 1;
}

static int resource_Text_setData(lua_State* L) {
    ShadyCore::TextResource* resource = Stack<ShadyCore::TextResource*>::get(L, 1);
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
    if (size != resource->length) resource->initialize(size);
    memcpy(resource->data, data, size);
    return 0;
}

// static const char* resource_Label_getName(const ShadyCore::LabelResource* resource) {
//     return resource->getName() ? resource->getName() : "";
// }

// static void resource_Label_setName(ShadyCore::LabelResource* resource, const char* name) {
//     resource->initialize(name);
// }

static int resource_Palette_getData(lua_State* L) {
    ShadyCore::Palette* resource = Stack<ShadyCore::Palette*>::get(L, 1);
    lua_pushlstring(L, (const char*)resource->data, 256 * 3);
    return 1;
}

static int resource_Palette_setData(lua_State* L) {
    ShadyCore::Palette* resource = Stack<ShadyCore::Palette*>::get(L, 1);
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
    if (size < 256 * 3) return luaL_error(L, "Palette must have 256 * 3 Bytes. Received %d.", size);
    memcpy(resource->data, data, 256 * 3);
    return 0;
}

namespace {
    class ImageProxy : public ShadyCore::Image {
    public: inline ImageProxy(uint32_t w, uint32_t h, uint8_t bpp) {
        initialize(w, h, std::ceil(w / 4.f) * 4, bpp);
    } };
}

static int resource_Image_getRaw(lua_State* L) {
    ShadyCore::Image* resource = Stack<ShadyCore::Image*>::get(L, 1);
    size_t size = resource->width * resource->height * (resource->bitsPerPixel == 8 ? 1 : 4);
    lua_pushlstring(L, (char*)resource->raw, size);
    return 1;
}

static int resource_Image_setRaw(lua_State* L) {
    ShadyCore::Image* resource = Stack<ShadyCore::Image*>::get(L, 1);
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
    size_t iSize = resource->width * resource->height * (resource->bitsPerPixel == 8 ? 1 : 4);
    if (size < iSize) return luaL_error(L, "Image must have %d Bytes. Received %d.", iSize, size);
    memcpy(resource->raw, data, iSize);
    return 0;
}

static int resource_Sfx_getData(lua_State* L) {
    ShadyCore::Sfx* resource = Stack<ShadyCore::Sfx*>::get(L, 1);
    if (!resource->data) lua_pushstring(L, "");
    else lua_pushlstring(L, (char*)resource->data, resource->size);
    return 1;
}

static int resource_Sfx_setData(lua_State* L) {
    ShadyCore::Sfx* resource = Stack<ShadyCore::Sfx*>::get(L, 1);
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
    if (size != resource->size) resource->initialize(size);
    memcpy(resource->data, data, size);
    return 0;
}

void ShadyLua::LualibResource(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("resource")
            .beginNamespace("Type")
                .addVariable("Unknown",     enumMap<ShadyCore::FileType::TYPE_UNKNOWN>(), false)
                .addVariable("Text",        enumMap<ShadyCore::FileType::TYPE_TEXT>(), false)
                .addVariable("Table",       enumMap<ShadyCore::FileType::TYPE_TABLE>(), false)
                .addVariable("Label",       enumMap<ShadyCore::FileType::TYPE_LABEL>(), false)
                .addVariable("Image",       enumMap<ShadyCore::FileType::TYPE_IMAGE>(), false)
                .addVariable("Palette",     enumMap<ShadyCore::FileType::TYPE_PALETTE>(), false)
                .addVariable("Sfx",         enumMap<ShadyCore::FileType::TYPE_SFX>(), false)
                .addVariable("Bgm",         enumMap<ShadyCore::FileType::TYPE_BGM>(), false)
                .addVariable("Schema",      enumMap<ShadyCore::FileType::TYPE_SCHEMA>(), false)
                .addVariable("Texture",     enumMap<ShadyCore::FileType::TYPE_TEXTURE>(), false)
            .endNamespace()
            .beginNamespace("Format")
                .addVariable("Unknown",     enumMap<ShadyCore::FileType::FORMAT_UNKNOWN>(), false)
                .addVariable("TextGame",    enumMap<ShadyCore::FileType::TEXT_GAME>(), false)
                .addVariable("TextNormal",  enumMap<ShadyCore::FileType::TEXT_NORMAL>(), false)
                .addVariable("TableGame",   enumMap<ShadyCore::FileType::TABLE_GAME>(), false)
                .addVariable("TableCsv",    enumMap<ShadyCore::FileType::TABLE_CSV>(), false)
                .addVariable("LabelRiff",   enumMap<ShadyCore::FileType::LABEL_RIFF>(), false)
                .addVariable("LabelLbl",    enumMap<ShadyCore::FileType::LABEL_LBL>(), false)
                .addVariable("ImageGame",   enumMap<ShadyCore::FileType::IMAGE_GAME>(), false)
                .addVariable("ImagePng",    enumMap<ShadyCore::FileType::IMAGE_PNG>(), false)
                .addVariable("ImageBmp",    enumMap<ShadyCore::FileType::IMAGE_BMP>(), false)
                .addVariable("PalettePal",  enumMap<ShadyCore::FileType::PALETTE_PAL>(), false)
                .addVariable("PaletteAct",  enumMap<ShadyCore::FileType::PALETTE_ACT>(), false)
                .addVariable("SfxGame",     enumMap<ShadyCore::FileType::SFX_GAME>(), false)
                .addVariable("SfxWave",     enumMap<ShadyCore::FileType::SFX_WAV>(), false)
                .addVariable("BgmOgg",      enumMap<ShadyCore::FileType::BGM_OGG>(), false)
                .addVariable("TextureDds",  enumMap<ShadyCore::FileType::TEXTURE_DDS>(), false)
                .addVariable("SchemaXml",   enumMap<ShadyCore::FileType::SCHEMA_XML>(), false)
                .addVariable("SchemaGui",   enumMap<ShadyCore::FileType::SCHEMA_GAME_GUI>(), false)
                .addVariable("SchemaAnim",  enumMap<ShadyCore::FileType::SCHEMA_GAME_ANIM>(), false)
                .addVariable("SchemaPat",   enumMap<ShadyCore::FileType::SCHEMA_GAME_PATTERN>(), false)
            .endNamespace()
            .addFunction("createfromfile", resource_createfromfile)
            .addFunction("export", resource_export)
            // .beginClass<ShadyCore::Resource>("Resource").endClass()
            // .deriveClass<ShadyCore::TextResource, ShadyCore::Resource>("Text")
            //     .addConstructor<void(*)(), RefCountedPtr<ShadyCore::TextResource>>()
            //     .addProperty("data", resource_Text_getData, resource_Text_setData)
            // .endClass()
            // .deriveClass<ShadyCore::LabelResource, ShadyCore::Resource>("Label")
            //     .addConstructor<void(*)(), RefCountedPtr<ShadyCore::LabelResource>>()
            //     .addProperty("begin", &ShadyCore::LabelResource::begin)
            //     .addProperty("end", &ShadyCore::LabelResource::end)
            // .endClass()
            // .deriveClass<ShadyCore::Palette, ShadyCore::Resource>("Palette")
            //     .addConstructor<void(*)(), RefCountedPtr<ShadyCore::Palette>>()
            //     .addProperty("data", resource_Palette_getData, resource_Palette_setData)
            //     // .addFunction("getColor", &ShadyCore::Palette::getColor)
            //     // .addFunction("setColor", &ShadyCore::Palette::setColor)
            //     // .addStaticFunction("packColor", &ShadyCore::Palette::packColor)
            //     .addStaticFunction("unpackColor", &ShadyCore::Palette::unpackColor)
            // .endClass()
            // .deriveClass<ImageProxy, ShadyCore::Resource>("Image")
            //     .addConstructor<void(*)(uint32_t, uint32_t, uint8_t), RefCountedPtr<ImageProxy>>()
            //     .addProperty<uint32_t>("width", &ImageProxy::width, false)
            //     .addProperty<uint32_t>("height", &ImageProxy::height, false)
            //     .addProperty<uint32_t>("paddedwidth", &ImageProxy::paddedWidth, false)
            //     .addProperty<uint8_t>("bitsperpixel", &ImageProxy::bitsPerPixel, false)
            //     .addProperty("raw", resource_Image_getRaw, resource_Image_setRaw)
            // .endClass()
            // .deriveClass<ShadyCore::Sfx, ShadyCore::Resource>("Sfx")
            //     .addConstructor<void(*)(), RefCountedPtr<ShadyCore::Sfx>>()
            //     .addProperty("channels", &ShadyCore::Sfx::channels)
            //     .addProperty("samplerate", &ShadyCore::Sfx::sampleRate)
            //     .addProperty("byterate", &ShadyCore::Sfx::byteRate)
            //     .addProperty("blockalign", &ShadyCore::Sfx::blockAlign)
            //     .addProperty("bitspersample", &ShadyCore::Sfx::bitsPerSample)
            //     .addProperty("data", resource_Sfx_getData, resource_Sfx_setData)
            // .endClass()
        .endNamespace()
    ;
}