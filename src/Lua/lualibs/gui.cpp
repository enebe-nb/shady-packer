#include "gui.hpp"
#include "../logger.hpp"
#include <LuaBridge/RefCountedPtr.h>
#include "../script.hpp"

using namespace luabridge;

namespace {
    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}
}

static int gui_OpenMenu(lua_State* L) {
    auto processHandler = lua_gettop(L) >= 1 && lua_isfunction(L, 1) ? luaL_ref(L, LUA_REGISTRYINDEX) : LUA_REFNIL;
    auto menu = new ShadyLua::MenuProxy(processHandler, L);
    SokuLib::activateMenu(menu);
    Stack<ShadyLua::MenuProxy*>::push(L, menu);
    return 1;
}

static bool gui_IsOpenMenu() {
    return SokuLib::menuManager.size();
}

static int gui_menu_getRenderer(lua_State* L) {
    if (lua_gettop(L) < 1) return luaL_error(L, "instance of gui.Menu not found");
    auto menu = Stack<ShadyLua::MenuProxy*>::get(L, 1);
    Stack<ShadyLua::Renderer*>::push(L, &menu->renderer);
    return 1;
}

static int gui_scene_getRenderer(lua_State* L) {
    if (lua_gettop(L) < 1) return luaL_error(L, "instance of gui.Scene not found");
    auto menu = Stack<ShadyLua::SceneProxy*>::get(L, 1);
    Stack<ShadyLua::Renderer*>::push(L, &menu->renderer);
    return 1;
}

template <typename T>
static T* gui_design_GetItemById(SokuLib::CDesign* schema, int id) {
    SokuLib::CDesign::Object* object; schema->getById(&object, id);
    return (T*)object;
}

static SokuLib::CDesign::Object* gui_design_GetItem(SokuLib::CDesign* schema, int index) {
    if (index <= 0) return nullptr;
    // Note: lua indexes start on 1
    auto it = schema->objects.begin(); while(--index) ++it;
    return *it;
}

static int gui_design_GetItemCount(SokuLib::CDesign* schema) {
    return schema->objects.size();
}

static int gui_getInput(lua_State* L) {
    luabridge::push(L, reinterpret_cast<SokuLib::KeyInput*(*)()>(0x43e060)());
    return 1;
}

ShadyLua::SpriteProxy::~SpriteProxy() {
    // for proxies a texture is created for each instance
    if (dxHandle) SokuLib::textureMgr.remove(dxHandle);
    dxHandle = 0;
}

//(const char* name = 0, int offsetX = 0, int offsetY = 0, int width = -1, int height = -1, int anchorX = 0, int anchorY = 0);
int ShadyLua::SpriteProxy::setTexture3(lua_State* L) {
    const int argc = lua_gettop(L);
    const char* name = (argc < 1) ? nullptr : luaL_checkstring(L, 1);
    int oX = (argc < 2) ? 0 : luaL_checkinteger(L, 2);
    int oY = (argc < 3) ? 0 : luaL_checkinteger(L, 3);
    int w = (argc < 4) ? -1 : luaL_checkinteger(L, 4);
    int h = (argc < 5) ? -1 : luaL_checkinteger(L, 5);
    int aX = (argc < 6) ? 0 : luaL_checkinteger(L, 6);
    int aY = (argc < 7) ? 0 : luaL_checkinteger(L, 7);
    if (dxHandle) SokuLib::textureMgr.remove(dxHandle);
    int texture; SokuLib::textureMgr.loadTexture(&texture, name, w == -1 ? &w : nullptr, h == -1 ? &h : nullptr);
    setTexture(texture, oX, oY, w, h, aX, aY);
    Stack<SpriteProxy*>::push(L, this);
    return 1;
}

//(const char* str, SokuLib::SWRFont &font, int width, int height);
int ShadyLua::SpriteProxy::setText(lua_State* L) {
    const int argc = lua_gettop(L);
    const char* str = luaL_checkstring(L, 1);
    auto font = Stack<ShadyLua::FontProxy*>::get(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    int outW, outH;
    if (dxHandle) SokuLib::textureMgr.remove(dxHandle);
    font->prepare();
    int texture; SokuLib::textureMgr.createTextTexture(&texture, str, *font->handles, w, h, &outW, &outH);
    setTexture2(texture, 0, 0, w, h);
    Stack<SpriteProxy*>::push(L, this);
    lua_pushinteger(L, outW);
    lua_pushinteger(L, outH);
    return 3;
}

ShadyLua::MenuCursorProxy::MenuCursorProxy(int w, bool horz, int max, int pos) {
    width = w;
    set(horz ? &SokuLib::inputMgrs.input.horizontalAxis :&SokuLib::inputMgrs.input.verticalAxis, max, pos);
    positions.resize(max);
}

void ShadyLua::MenuCursorProxy::setPosition(int i, int x, int y) {
    if (i < 0 || i >= positions.size()) throw std::exception("index out of range");
    positions[i].first = x; positions[i].second = y;
}

void ShadyLua::MenuCursorProxy::setRange(int x, int y, int dx, int dy) {
    for (int i = 0; i < positions.size(); ++i) {
        positions[i].first = x + i*dx; positions[i].second = y + i*dy;
    }
}

void ShadyLua::Renderer::update() {
    for(auto& cursor : cursors) if (cursor.active) cursor.update();
    effects.Update();
}

void ShadyLua::Renderer::render() {
    if (!isActive) return;

    for(auto& cursor : cursors) {
        cursor.render(cursor.positions[cursor.pos].first, cursor.positions[cursor.pos].second, cursor.width);
    }

    if (guiSchema.objects.size()) guiSchema.render4();
    for (auto i : activeLayers) {
        effects.Render(i);
        auto range = sprites.equal_range(i);
        for (auto iter = range.first; iter != range.second; ++iter) {
            iter->second.render(0, 0); // TODO check position
        }
    }
}

//(const char* name = 0, int offsetX = 0, int offsetY = 0, int width = -1, int height = -1, int anchorX = 0, int anchorY = 0, int layer = 0);
int ShadyLua::Renderer::createSprite(lua_State* L) {
    const int argc = lua_gettop(L);
    const char* name = (argc < 1) ? nullptr : luaL_checkstring(L, 2);
    int oX = (argc < 3) ? 0 : luaL_checkinteger(L, 3);
    int oY = (argc < 4) ? 0 : luaL_checkinteger(L, 4);
    int w = (argc < 5) ? -1 : luaL_checkinteger(L, 5);
    int h = (argc < 6) ? -1 : luaL_checkinteger(L, 6);
    int aX = (argc < 7) ? 0 : luaL_checkinteger(L, 7);
    int aY = (argc < 8) ? 0 : luaL_checkinteger(L, 8);
    int layer = (argc < 9) ? 0 : luaL_checkinteger(L, 9);
    auto& sprite = this->sprites.emplace(std::piecewise_construct, std::forward_as_tuple(layer), std::forward_as_tuple())->second;
    activeLayers.insert(layer);
    int texture; SokuLib::textureMgr.loadTexture(&texture, name, w == -1 ? &w : nullptr, h == -1 ? &h : nullptr);
    sprite.setTexture(texture, oX, oY, w, h, aX, aY);
    Stack<SpriteProxy*>::push(L, &sprite);
    activeLayers.insert(layer);
    return 1;
}

//(const char* str, SokuLib::SWRFont &font, int width, int height, int layer = 0);
int ShadyLua::Renderer::createText(lua_State* L) {
    const int argc = lua_gettop(L);
    const char* str = luaL_checkstring(L, 2);
    auto font = Stack<ShadyLua::FontProxy*>::get(L, 3);
    int w = luaL_checkinteger(L, 4);
    int h = luaL_checkinteger(L, 5);
    int layer = (argc < 6) ? 0 : luaL_checkinteger(L, 6);
    int outW, outH;
    auto& sprite = this->sprites.emplace(std::piecewise_construct, std::forward_as_tuple(layer), std::forward_as_tuple())->second;
    font->prepare();
    int texture; SokuLib::textureMgr.createTextTexture(&texture, str, *font->handles, w, h, &outW, &outH);
    sprite.setTexture2(texture, 0, 0, w, h);
    Stack<SpriteProxy*>::push(L, &sprite);
    lua_pushinteger(L, outW);
    lua_pushinteger(L, outH);
    activeLayers.insert(layer);
    return 3;
}

//(int id, float x, float y, char dir = 1, char layer = 0);
int ShadyLua::Renderer::createEffect(lua_State* L) {
    const int argc = lua_gettop(L);
    int id = luaL_checkinteger(L, 2);
    float x = (argc < 3) ? 0 : luaL_checknumber(L, 3);
    float y = (argc < 4) ? 0 : luaL_checknumber(L, 4);
    int dir = (argc < 5) ? 1 : luaL_checkinteger(L, 5);
    int layer = (argc < 6) ? 0 : luaL_checkinteger(L, 6);
    Effect* effect = (Effect*) effects.CreateEffect(id, x, y, (SokuLib::Direction)dir, layer, 0);
    Stack<Effect*>::push(L, effect);
    activeLayers.insert(layer);
    return 1;
}

//(int width, int max = 1, int pos = 0);
template<bool HORZ>
int ShadyLua::Renderer::createCursor(lua_State* L) {
    const int argc = lua_gettop(L);
    int width = luaL_checkinteger(L, 2);
    int max = (argc < 3) ? 1 : luaL_checkinteger(L, 3);
    int pos = (argc < 4) ? 0 : luaL_checkinteger(L, 4);
    auto& cursor = cursors.emplace_back(width, HORZ, max, pos);
    Stack<MenuCursorProxy*>::push(L, &cursor);
    return 1;
}

int ShadyLua::Renderer::destroy(lua_State* L) {
    const int argc = lua_gettop(L);
    if (argc < 2) return 0;
    for (int i = 2; i <= argc; ++i) {
        if (Stack<SpriteProxy*>::Helper::isInstance(L, i)) {
            auto sprite = Stack<SpriteProxy*>::get(L, i);
            for (auto iter = sprites.begin(); iter != sprites.end(); ++iter) {
                if (sprite == &iter->second) { sprites.erase(iter); break; }
            }
        } else if (Stack<Effect*>::Helper::isInstance(L, i)) {
            throw std::exception("TODO destruction of effects is not implemented.");
        } else if (Stack<MenuCursorProxy*>::Helper::isInstance(L, i)) {
            auto cursor = Stack<MenuCursorProxy*>::get(L, i);
            for (auto iter = cursors.begin(); iter != cursors.end(); ++iter) {
                if (cursor == iter.operator->()) { cursors.erase(iter); break; }
            }
        }
    }
}

void ShadyLua::Renderer::clear() {
    cursors.clear();
    guiSchema.clear();
    sprites.clear();
    effects.ClearPattern();
    activeLayers.clear();
}

ShadyLua::MenuProxy::MenuProxy(int handler, lua_State* L)
    : processHandler(handler), data(L), script(ShadyLua::ScriptMap[L]) {}

void ShadyLua::MenuProxy::_() {}
int ShadyLua::MenuProxy::onProcess() {
    int ret = 1;
    renderer.update();
    if (processHandler != LUA_REFNIL) { 
        std::lock_guard scriptGuard(script->mutex);
        lua_State* L = script->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, processHandler);
        Stack<MenuProxy*>::push(L, this);
        if (lua_pcall(L, 1, 1, 0)) {
            Logger::Error(lua_tostring(L, -1));
        } else if (lua_isnumber(L, -1)) {
            ret = lua_tointeger(L, -1);
        } lua_pop(L, 1);
    }
    return ret;
}

int ShadyLua::MenuProxy::onRender() {
    renderer.render();
    return 1;
}

bool ShadyLua::MenuProxy::ShowMessage(const char* text) {
    reinterpret_cast<void (__stdcall *)(const char*)>(0x443900)(text);
    return true;
}

std::unordered_multimap<SokuLib::IScene*, ShadyLua::SceneProxy*> ShadyLua::SceneProxy::listeners;
ShadyLua::SceneProxy::SceneProxy(lua_State* L) : data(L), script(ShadyLua::ScriptMap[L]) {}
int ShadyLua::SceneProxy::onProcess() {
    renderer.update();
    if (processHandler != LUA_REFNIL) { 
        std::lock_guard scriptGuard(script->mutex);
        lua_State* L = script->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, processHandler);
        Stack<SceneProxy*>::push(L, this);
        if (lua_pcall(L, 1, 1, 0)) {
            Logger::Error(lua_tostring(L, -1));
            lua_pop(L, 1);
        } else if (lua_isnumber(L, -1)) {
            auto ret = lua_tointeger(L, -1);
            lua_pop(L, 1);
            return ret;
        }
    }
    return -1;
}

void ShadyLua::LualibGui(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("gui")
            .addCFunction("OpenMenu", gui_OpenMenu)
            .addFunction("IsOpenMenu", gui_IsOpenMenu)
            .beginClass<ShadyLua::Renderer>("Renderer")
                .addData("Schema", &ShadyLua::Renderer::guiSchema, false)
                .addData("IsActive", &ShadyLua::Renderer::isActive, true)

                .addFunction("LoadEffectPattern", &ShadyLua::Renderer::LoadEffectPattern)
                .addFunction("CreateSprite", &ShadyLua::Renderer::createSprite)
                .addFunction("CreateText", &ShadyLua::Renderer::createText)
                .addFunction("CreateEffect", &ShadyLua::Renderer::createEffect)
                .addFunction("CreateCursorH", &ShadyLua::Renderer::createCursor<true>)
                .addFunction("CreateCursorV", &ShadyLua::Renderer::createCursor<false>)

                .addFunction("Destroy", &ShadyLua::Renderer::destroy) // TODO test to set nil to things
            .endClass()
            .beginClass<ShadyLua::MenuProxy>("Menu")
                .addProperty("Renderer", gui_menu_getRenderer, 0)
                .addProperty("Input", gui_getInput, 0)
                .addData("Data", &MenuProxy::data, true)

                .addFunction("ShowMessage", &MenuProxy::ShowMessage)
            .endClass()
            .beginClass<ShadyLua::SceneProxy>("Scene")
                .addProperty("Renderer", gui_scene_getRenderer, 0)
                .addProperty("Input", gui_getInput, 0)
            .endClass()
            .beginClass<SokuLib::CDesign>("Design")
                .addConstructor<void(*)(void), RefCountedPtr<SokuLib::CDesign> >()
                .addFunction("SetColor", &SokuLib::CDesign::setColor)
                .addFunction("LoadResource", &SokuLib::CDesign::loadResource)
                .addFunction("Clear", &SokuLib::CDesign::clear)
                .addFunction("GetGaugeById", gui_design_GetItemById<SokuLib::CDesign::Gauge>)
                .addFunction("GetNumberById", gui_design_GetItemById<SokuLib::CDesign::Number>)
                .addFunction("GetSpriteById", gui_design_GetItemById<SokuLib::CDesign::Sprite>)
                .addFunction("GetObjectById", gui_design_GetItemById<SokuLib::CDesign::Object>)
                .addFunction("GetItem", gui_design_GetItem)
                .addFunction("GetItemCount", gui_design_GetItemCount)
            .endClass()
            .beginClass<SokuLib::KeyInput>("KeyInput")
                .addData("HAxis", &SokuLib::KeyInput::horizontalAxis, false)
                .addData("VAxis", &SokuLib::KeyInput::verticalAxis, false)
                .addData("A", &SokuLib::KeyInput::a, false)
                .addData("B", &SokuLib::KeyInput::b, false)
                .addData("C", &SokuLib::KeyInput::c, false)
                .addData("D", &SokuLib::KeyInput::d, false)
                .addData("Change", &SokuLib::KeyInput::changeCard, false)
                .addData("Spell", &SokuLib::KeyInput::spellcard, false)
            .endClass()
            .beginClass<ShadyLua::SpriteProxy>("Sprite")
                .addData("IsEnabled", &ShadyLua::SpriteProxy::isEnabled, true)
                .addData("Position", &ShadyLua::SpriteProxy::pos, true)
                .addData("Scale", &ShadyLua::SpriteProxy::scale, true)
                .addData("Rotation", &ShadyLua::SpriteProxy::rotation, true)
                .addFunction("SetColor", &ShadyLua::SpriteProxy::setColor)
                .addFunction("SetTexture", &ShadyLua::SpriteProxy::setTexture3)
                .addFunction("SetText", &ShadyLua::SpriteProxy::setText)
            .endClass()
            .beginClass<ShadyLua::MenuCursorProxy>("Cursor")
                .addData("Active", &MenuCursorProxy::active, true)
                .addData("Index", &MenuCursorProxy::pos, true)
                .addFunction("SetPosition", &MenuCursorProxy::setPosition)
                .addFunction("SetRange", &MenuCursorProxy::setRange)
            .endClass()
            .beginClass<ShadyLua::Renderer::Effect>("Effect")
                .addData("IsEnabled", &ShadyLua::Renderer::Effect::isActive, true)
                .addData("IsAlive", &ShadyLua::Renderer::Effect::unknown158, true)
                .addData("Position", &ShadyLua::Renderer::Effect::position, true)
                .addData("Speed", &ShadyLua::Renderer::Effect::speed, true)
                .addData("Gravity", &ShadyLua::Renderer::Effect::gravity, true)
                .addData("Center", &ShadyLua::Renderer::Effect::center, true)
                // TODO render options
                .addFunction("SetActionSequence", &ShadyLua::Renderer::Effect::setActionSequence)
                .addFunction("SetAction", &ShadyLua::Renderer::Effect::setAction)
                .addFunction("SetSequence", &ShadyLua::Renderer::Effect::setSequence)
                .addFunction("ResetSequence", &ShadyLua::Renderer::Effect::resetSequence)
                .addFunction("PrevSequence", &ShadyLua::Renderer::Effect::prevSequence)
                .addFunction("NextSequence", &ShadyLua::Renderer::Effect::nextSequence)
                .addFunction("SetPose", &ShadyLua::Renderer::Effect::setPose)
                .addFunction("PrevPose", &ShadyLua::Renderer::Effect::prevPose)
                .addFunction("NextPose", &ShadyLua::Renderer::Effect::nextPose)
            .endClass()
            .beginClass<ShadyLua::FontProxy>("Font")
                .addConstructor<void(*)(void), RefCountedPtr<ShadyLua::FontProxy>>()
                .addFunction("SetFontName", &ShadyLua::FontProxy::setFontName)
                .addFunction("SetColor", &ShadyLua::FontProxy::setColor)
                .addProperty("Height", &ShadyLua::FontProxy::height, true)
                .addProperty("Weight", &ShadyLua::FontProxy::weight, true)
                .addProperty("Italic", &ShadyLua::FontProxy::italic, true)
                .addProperty("Shadow", &ShadyLua::FontProxy::shadow, true)
                .addProperty("Wrap", &ShadyLua::FontProxy::useOffset, true)
                .addProperty("OffsetX", &ShadyLua::FontProxy::offsetX, true)
                .addProperty("OffsetY", &ShadyLua::FontProxy::offsetY, true)
                .addProperty("SpacingX", &ShadyLua::FontProxy::charSpaceX, true)
                .addProperty("SpacingY", &ShadyLua::FontProxy::charSpaceY, true)
            .endClass()
        .endNamespace()
    ;
}