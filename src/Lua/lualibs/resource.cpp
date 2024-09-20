#include "../lualibs.hpp"
#include "../script.hpp"
#include "../../Core/resource.hpp"
#include "../../Core/resource/readerwriter.hpp"
#include "soku.hpp"
#include <LuaBridge/LuaBridge.h>
#include <sstream>
#include <fstream>
#include <cmath>
#include <deque>
#include <streambuf>

using namespace luabridge;

namespace {
    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}

    template <class T> class _ResourceProxy : public ShadyLua::ResourceProxy {
        static_assert(std::is_base_of<ShadyCore::Resource, T>::value);
        static const ShadyCore::FileType::Type staticType;
    public:
        _ResourceProxy();
        inline T& get() { return *reinterpret_cast<T*>(resource); }
        inline const T& get() const { return *reinterpret_cast<T*>(resource); }
    };

    const ShadyCore::FileType::Type _ResourceProxy<ShadyCore::TextResource>::staticType =   ShadyCore::FileType::TYPE_TEXT;
    const ShadyCore::FileType::Type _ResourceProxy<ShadyCore::LabelResource>::staticType =  ShadyCore::FileType::TYPE_LABEL;
    const ShadyCore::FileType::Type _ResourceProxy<ShadyCore::Palette>::staticType =        ShadyCore::FileType::TYPE_PALETTE;
    const ShadyCore::FileType::Type _ResourceProxy<ShadyCore::Image>::staticType =          ShadyCore::FileType::TYPE_IMAGE;
    const ShadyCore::FileType::Type _ResourceProxy<ShadyCore::Sfx>::staticType =            ShadyCore::FileType::TYPE_SFX;
}

void ShadyLua::ResourceProxy::push(lua_State* L) {
    switch(type) {
        case ShadyCore::FileType::Type::TYPE_TEXT:
        case ShadyCore::FileType::Type::TYPE_TABLE:
            return Stack<_ResourceProxy<ShadyCore::TextResource>*>::push(L, reinterpret_cast<_ResourceProxy<ShadyCore::TextResource>*>(this));
        case ShadyCore::FileType::Type::TYPE_LABEL:
            return Stack<_ResourceProxy<ShadyCore::LabelResource>*>::push(L, reinterpret_cast<_ResourceProxy<ShadyCore::LabelResource>*>(this));
        case ShadyCore::FileType::Type::TYPE_PALETTE:
            return Stack<_ResourceProxy<ShadyCore::Palette>*>::push(L, reinterpret_cast<_ResourceProxy<ShadyCore::Palette>*>(this));
        case ShadyCore::FileType::Type::TYPE_IMAGE:
            return Stack<_ResourceProxy<ShadyCore::Image>*>::push(L, reinterpret_cast<_ResourceProxy<ShadyCore::Image>*>(this));
        case ShadyCore::FileType::Type::TYPE_SFX:
            return Stack<_ResourceProxy<ShadyCore::Sfx>*>::push(L, reinterpret_cast<_ResourceProxy<ShadyCore::Sfx>*>(this));
        default:
            throw std::runtime_error("ResourceProxy: resource type not supported (yet?).");
    }
}

static ShadyCore::FileType getTypeAndFormat(const char* filename) {
	ShadyCore::FileType ft = ShadyCore::FileType::get(filename);
	if (ft.format != ShadyCore::FileType::FORMAT_UNKNOWN) return ft;

	switch(ft.type) {
	case ShadyCore::FileType::TYPE_TEXT:
		ft.format = ft.extValue == ShadyCore::FileType::getExtValue(".cv0")
			? ShadyCore::FileType::TEXT_GAME : ShadyCore::FileType::TEXT_NORMAL;
		break;
	case ShadyCore::FileType::TYPE_TABLE:
		ft.format = ft.extValue == ShadyCore::FileType::getExtValue(".cv1")
			? ShadyCore::FileType::TABLE_GAME : ShadyCore::FileType::TABLE_CSV;
		break;
	case ShadyCore::FileType::TYPE_LABEL:
		ft.format = ft.extValue == ShadyCore::FileType::getExtValue(".sfl")
			? ShadyCore::FileType::LABEL_RIFF : ShadyCore::FileType::LABEL_LBL;
		break;
	case ShadyCore::FileType::TYPE_IMAGE:
		ft.format = ft.extValue == ShadyCore::FileType::getExtValue(".cv2")
			? ShadyCore::FileType::IMAGE_GAME : ShadyCore::FileType::getExtValue(".png")
			? ShadyCore::FileType::IMAGE_PNG : ShadyCore::FileType::IMAGE_BMP;
		break;
	case ShadyCore::FileType::TYPE_PALETTE:
		ft.format = ft.extValue == ShadyCore::FileType::getExtValue(".pal")
			? ShadyCore::FileType::PALETTE_PAL : ShadyCore::FileType::PALETTE_ACT;
		break;
	case ShadyCore::FileType::TYPE_SFX:
		ft.format = ft.extValue == ShadyCore::FileType::getExtValue(".cv3")
			? ShadyCore::FileType::SFX_GAME : ShadyCore::FileType::SFX_GAME;
		break;
	case ShadyCore::FileType::TYPE_BGM:
		ft.format = ShadyCore::FileType::BGM_OGG;
		break;
	case ShadyCore::FileType::TYPE_TEXTURE:
		ft.format = ShadyCore::FileType::TEXTURE_DDS;
		break;
	case ShadyCore::FileType::TYPE_SCHEMA:
		if (ft.extValue == ShadyCore::FileType::getExtValue(".dat")) {
			ft.format = ShadyCore::FileType::SCHEMA_GAME_GUI;
		} else if(ft.extValue == ShadyCore::FileType::getExtValue(".pat")) {
			std::string_view name(filename);
			if (name.size() >= 6 && name.ends_with("effect")
				|| name.size() >= 5 && name.ends_with("stand"))
				ft.format = ShadyCore::FileType::SCHEMA_GAME_ANIM;
			else ft.format = ShadyCore::FileType::SCHEMA_GAME_PATTERN;
		} else if(ft.extValue == ShadyCore::FileType::getExtValue(".xml")) {
			ft.format = ShadyCore::FileType::SCHEMA_XML;
		}
		break;
	}

	return ft;
}

/** Creates a resource from a file in script space */
static int resource_createfromfile(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    ShadyLua::LuaScript* script = ShadyLua::ScriptMap.at(L);

    auto file = script->openFile(filename);
    if (!file) return luaL_error(L, "Cannot open file.");
    auto ft = getTypeAndFormat(filename);
    ShadyLua::ResourceProxy* proxy = new ShadyLua::ResourceProxy(ShadyCore::createResource(ft.type), ft.type, true);
    ShadyCore::getResourceReader(ShadyCore::FileType(ft.type, ft.format))(proxy->resource, file->input);
    script->closeFile(file);
    proxy->push(L);
    return 1;
}

/** Saves a resource into filesystem */
static void resource_export(RefCountedObjectPtr<ShadyLua::ResourceProxy> resource, const char* filename, lua_State* L) {
    std::fstream out(filename, std::ios::out|std::ios::binary);
    auto ft = getTypeAndFormat(filename);
    if (ft.type != resource->type) luaL_error(L, "Resource type doesn't match the filename");
    ShadyCore::getResourceWriter(ShadyCore::FileType(resource->type, ft.format))(resource->resource, out);
}

template<> _ResourceProxy<ShadyCore::TextResource>::_ResourceProxy() : ResourceProxy(staticType) { get().initialize(0); }

static int resource_Text_getData(lua_State* L) {
    auto resource = &Stack<_ResourceProxy<ShadyCore::TextResource>*>::get(L, 1)->get();
    if (!resource->data) lua_pushstring(L, "");
    else lua_pushlstring(L, resource->data, resource->length);
    return 1;
}

static int resource_Text_setData(lua_State* L) {
    auto resource = &Stack<_ResourceProxy<ShadyCore::TextResource>*>::get(L, 1)->get();
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
    if (size != resource->length) resource->initialize(size);
    memcpy(resource->data, data, size);
    return 0;
}

template<> _ResourceProxy<ShadyCore::LabelResource>::_ResourceProxy() : ResourceProxy(staticType) {}
static inline int resource_Label_getBegin(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::LabelResource>*>::get(L, 1)->get().begin); return 1; }
static inline int resource_Label_getEnd(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::LabelResource>*>::get(L, 1)->get().end); return 1; }
static inline int resource_Label_setBegin(lua_State* L) { Stack<_ResourceProxy<ShadyCore::LabelResource>*>::get(L, 1)->get().begin = luaL_checknumber(L, 2); return 0; }
static inline int resource_Label_setEnd(lua_State* L) { Stack<_ResourceProxy<ShadyCore::LabelResource>*>::get(L, 1)->get().end = luaL_checknumber(L, 2); return 0; }

template<> _ResourceProxy<ShadyCore::Palette>::_ResourceProxy() : ResourceProxy(staticType) { get().initialize(16); }

static int resource_Palette_getData(lua_State* L) {
	auto resource = &Stack<_ResourceProxy<ShadyCore::Palette>*>::get(L, 1)->get();
	lua_pushlstring(L, (const char*)resource->data, resource->getDataSize());
    return 1;
}

static int resource_Palette_setData(lua_State* L) { // TODO better control and true colors
	auto resource = &Stack<_ResourceProxy<ShadyCore::Palette>*>::get(L, 1)->get();
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
	if (size < 256 * 2) return luaL_error(L, "Palette must have 256 * 2 Bytes. Received %d.", size);
	if (size != resource->getDataSize()) resource->initialize(16);
	memcpy(resource->data, data, resource->getDataSize());
    return 0;
}

static int resource_Palette_getColor(_ResourceProxy<ShadyCore::Palette>* pal, lua_State* L) {
    int i = luaL_checkinteger(L, 2) - 1;
    uint16_t color = pal->get().bitsPerPixel > 16 ? pal->get().packColor(((uint32_t*)pal->get().data)[i]) : ((uint16_t*)pal->get().data)[i];
    lua_pushboolean(L, (color >> 15) & 0x01);
    lua_pushinteger(L, (color >> 10) & 0x1f);
    lua_pushinteger(L, (color >>  5) & 0x1f);
    lua_pushinteger(L, (color >>  0) & 0x1f);
    return 4;
}

static int resource_Palette_setColor(_ResourceProxy<ShadyCore::Palette>* pal, lua_State* L) {
    const int i = luaL_checkinteger(L, 2) - 1;
    const bool hadBool = lua_isboolean(L, 3);
    const bool a = hadBool ? lua_toboolean(L, 3) : true;
    const int r = luaL_optinteger(L, hadBool ? 4 : 3, 0) & 0x1f;
    const int g = luaL_optinteger(L, hadBool ? 5 : 4, 0) & 0x1f;
    const int b = luaL_optinteger(L, hadBool ? 6 : 5, 0) & 0x1f;
    uint16_t color = (((int)a) << 15) | (r << 10) | (g << 5) | b;
    if (pal->get().bitsPerPixel > 16) ((uint32_t*)pal->get().data)[i] = pal->get().unpackColor(color);
    else ((uint16_t*)pal->get().data)[i] = color;
    return 0;
}

template<> _ResourceProxy<ShadyCore::Image>::_ResourceProxy() : ResourceProxy(staticType) { }
static inline int resource_Image_getBPP(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get().bitsPerPixel); return 1; }
static inline int resource_Image_getWidth(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get().width); return 1; }
static inline int resource_Image_getHeight(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get().height); return 1; }
static inline int resource_Image_getPaddedWidth(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get().paddedWidth); return 1; }
static inline int resource_Image_getSize(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get().getRawSize()); return 1; }

static void resource_Image_create(_ResourceProxy<ShadyCore::Image>* image, int b, int w, int h) {
    image->get().initialize(b, w, h, ((w+3)/4)*4);
}

static int resource_Image_getRaw(lua_State* L) {
    auto resource = &Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get();
    if (!resource->raw) lua_pushnil(L);
    else lua_pushlstring(L, (char*)resource->raw, resource->getRawSize());
    return 1;
}

static int resource_Image_setRaw(lua_State* L) {
    auto resource = &Stack<_ResourceProxy<ShadyCore::Image>*>::get(L, 1)->get();
    if (!resource->raw) return luaL_error(L, "Image must be created.");
    size_t size; const char* data = luaL_checklstring(L, 2, &size);
    size_t iSize = resource->getRawSize();
    if (size < iSize) return luaL_error(L, "Image must have %d Bytes. Received %d.", iSize, size);
    memcpy(resource->raw, data, iSize);
    return 0;
}

template<> _ResourceProxy<ShadyCore::Sfx>::_ResourceProxy() : ResourceProxy(staticType) { get().initialize(0); }
static inline int resource_Sfx_getChannels(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().channels); return 1; }
static inline int resource_Sfx_getSampleRate(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().sampleRate); return 1; }
static inline int resource_Sfx_getByteRate(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().byteRate); return 1; }
static inline int resource_Sfx_getBlockAlign(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().blockAlign); return 1; }
static inline int resource_Sfx_getBPS(lua_State* L) { push(L, Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().bitsPerSample); return 1; }
static inline int resource_Sfx_setChannels(lua_State* L) { Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().channels = luaL_checkinteger(L, 2); return 0; }
static inline int resource_Sfx_setSampleRate(lua_State* L) { Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().sampleRate = luaL_checkinteger(L, 2); return 0; }
static inline int resource_Sfx_setByteRate(lua_State* L) { Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().byteRate = luaL_checkinteger(L, 2); return 0; }
static inline int resource_Sfx_setBlockAlign(lua_State* L) { Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().blockAlign = luaL_checkinteger(L, 2); return 0; }
static inline int resource_Sfx_setBPS(lua_State* L) { Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get().bitsPerSample = luaL_checkinteger(L, 2); return 0; }

static int resource_Sfx_getData(lua_State* L) {
	auto resource = &Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get();
    if (!resource->data) lua_pushstring(L, "");
    else lua_pushlstring(L, (char*)resource->data, resource->size);
    return 1;
}

static int resource_Sfx_setData(lua_State* L) {
	auto resource = &Stack<_ResourceProxy<ShadyCore::Sfx>*>::get(L, 1)->get();
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
            .addFunction("createfromfile", resource_createfromfile)
            .addFunction("export", resource_export)
            .beginClass<ResourceProxy>("Resource").endClass()
            .deriveClass<_ResourceProxy<ShadyCore::TextResource>, ResourceProxy>("Text")
                .addConstructor<void(*)(), RefCountedObjectPtr<_ResourceProxy<ShadyCore::TextResource>>>() // TODO test reference counter
                .addProperty("data", resource_Text_getData, resource_Text_setData)
            .endClass()
            .deriveClass<_ResourceProxy<ShadyCore::LabelResource>, ResourceProxy>("Label")
                .addConstructor<void(*)(), RefCountedObjectPtr<_ResourceProxy<ShadyCore::LabelResource>>>()
                .addProperty("begin", resource_Label_getBegin, resource_Label_setBegin)
                .addProperty("end", resource_Label_getEnd, resource_Label_setEnd)
            .endClass()
            .deriveClass<_ResourceProxy<ShadyCore::Palette>, ResourceProxy>("Palette")
                .addConstructor<void(*)(), RefCountedObjectPtr<_ResourceProxy<ShadyCore::Palette>>>()
                .addProperty("data", resource_Palette_getData, resource_Palette_setData)
                .addFunction("getColor", resource_Palette_getColor)
                .addFunction("setColor", resource_Palette_setColor)
            .endClass()
            .deriveClass<_ResourceProxy<ShadyCore::Image>, ResourceProxy>("Image")
                .addConstructor<void(*)(), RefCountedObjectPtr<_ResourceProxy<ShadyCore::Image>>>()
                .addFunction("create", resource_Image_create)
                .addProperty("width", resource_Image_getWidth, 0)
                .addProperty("height", resource_Image_getHeight, 0)
                .addProperty("paddedWidth", resource_Image_getPaddedWidth, 0)
                .addProperty("bitsPerPixel", resource_Image_getBPP, 0)
                .addProperty("size", resource_Image_getSize, 0)
                .addProperty("raw", resource_Image_getRaw, resource_Image_setRaw)
            .endClass()
            .deriveClass<_ResourceProxy<ShadyCore::Sfx>, ResourceProxy>("Sfx")
                .addConstructor<void(*)(), RefCountedObjectPtr<_ResourceProxy<ShadyCore::Sfx>>>()
                .addProperty("channels", resource_Sfx_getChannels, resource_Sfx_setChannels)
                .addProperty("sampleRate", resource_Sfx_getSampleRate, resource_Sfx_setSampleRate)
                .addProperty("byteRate", resource_Sfx_getByteRate, resource_Sfx_setByteRate)
                .addProperty("blockAlign", resource_Sfx_getBlockAlign, resource_Sfx_setBlockAlign)
                .addProperty("bitsPerSample", resource_Sfx_getBPS, resource_Sfx_setBPS)
                .addProperty("data", resource_Sfx_getData, resource_Sfx_setData)
            .endClass()
        .endNamespace()
    ;
}