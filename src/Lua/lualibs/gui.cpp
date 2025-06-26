#include "gui.hpp"
#include "../logger.hpp"
#include <LuaBridge/RefCountedPtr.h>
#include "../script.hpp"

using namespace luabridge;

namespace {
    template <int value> static inline int* enumMap()
        {static const int valueHolder = value; return (int*)&valueHolder;}

    struct ValueProxy {
        union { float number; int gauge; };
    };
}

static ValueProxy* gui_design_getValue(SokuLib::CDesign::Object* object) {
    auto value = new ValueProxy(); // <-- this have memory leak, but it's hard to prevent this
    if (*(int*)object == SokuLib::ADDR_VTBL_CDESIGN_GAUGE) {
        reinterpret_cast<SokuLib::CDesign::Gauge*>(object)->gauge.set(&value->gauge, 0, 100);
    } else if (*(int*)object == SokuLib::ADDR_VTBL_CDESIGN_NUMBER) {
        reinterpret_cast<SokuLib::CDesign::Number*>(object)->number.set(&value->number);
    }
    return value;
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
    auto scene = Stack<ShadyLua::SceneProxy*>::get(L, 1);
    Stack<ShadyLua::Renderer*>::push(L, &scene->renderer);
    return 1;
}

static int gui_renderer_getEffects(lua_State* L) {
    if (lua_gettop(L) < 1) return luaL_error(L, "instance of gui.Renderer not found");
    auto renderer = Stack<ShadyLua::Renderer*>::get(L, 1);
    Stack<ShadyLua::EffectManagerProxy*>::push(L, &renderer->effects);
    return 1;
}

static SokuLib::CDesign::Object* gui_design_GetItemById(SokuLib::CDesign* schema, int id) {
    SokuLib::CDesign::Object* object = 0; schema->getById(&object, id);
    return object;
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
    const char* name = (argc < 2) ? nullptr : luaL_checkstring(L, 2);
    int oX = (argc < 3) ? 0 : luaL_checkinteger(L, 3);
    int oY = (argc < 4) ? 0 : luaL_checkinteger(L, 4);
    int w = (argc < 5) ? -1 : luaL_checkinteger(L, 5);
    int h = (argc < 6) ? -1 : luaL_checkinteger(L, 6);
    int aX = (argc < 7) ? 0 : luaL_checkinteger(L, 7);
    int aY = (argc < 8) ? 0 : luaL_checkinteger(L, 8);
    if (dxHandle) SokuLib::textureMgr.remove(dxHandle);
    int texture; SokuLib::textureMgr.loadTexture(&texture, name, w == -1 ? &w : nullptr, h == -1 ? &h : nullptr);
    if (!texture) Logger::Warning("Texture not found: ", name);
    setTexture(texture, oX, oY, w, h, aX, aY);
    Stack<SpriteProxy*>::push(L, this);
    return 1;
}

//(const char* str, SokuLib::SWRFont &font, int width, int height);
int ShadyLua::SpriteProxy::setText(lua_State* L) {
    const int argc = lua_gettop(L);
    const char* str = luaL_checkstring(L, 2);
    auto font = Stack<ShadyLua::FontProxy*>::get(L, 3);
    int w = luaL_checkinteger(L, 4);
    int h = luaL_checkinteger(L, 5);
    int outW, outH;
    if (dxHandle) SokuLib::textureMgr.remove(dxHandle);
    font->prepare();
    int texture; SokuLib::textureMgr.createTextTexture(&texture, str, *font->handle, w, h, &outW, &outH);
    setTexture2(texture, 0, 0, w, h);
    Stack<SpriteProxy*>::push(L, this);
    lua_pushinteger(L, outW);
    lua_pushinteger(L, outH);
    return 3;
}

ShadyLua::MenuCursorProxy::MenuCursorProxy(int w, bool horz, int max, int pos) {
    width = w;
    valueAddr2 = !horz ? &SokuLib::inputMgrs.input.horizontalAxis : &SokuLib::inputMgrs.input.verticalAxis;
    set(horz ? &SokuLib::inputMgrs.input.horizontalAxis :&SokuLib::inputMgrs.input.verticalAxis, max, 0);
    setIndex(pos);
    positions.resize(max);
}
bool ShadyLua::MenuCursorProxy::updateProxy() {
    bool moving = MenuCursor::update();
    if (!dRows) pgPos = 0;//fix
    else if (valueAddr2) {
        auto c = abs(*valueAddr2);
        switch ((c == 1 || c > 30 && c % 6 == 0) ? *valueAddr2 / c : 0) {
        case -1:    pgUp(); moving = true; break;
        case 1:     pgDn(); moving = true; break;
        default: break;
        }
    }
    return moving;
}

void ShadyLua::MenuCursorProxy::setPageRows(int rows) {
    if (dRows == rows) return;

    dRows = rows;
    pgPos = 0;
    if (dRows > 0) positions.resize(dRows);
    else positions.resize(max);
}

int ShadyLua::MenuCursorProxy::setRange(lua_State* L) {
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int dx = luaL_optinteger(L, 4, 0);
    int dy = luaL_optinteger(L, 5, 0);
    if (lua_gettop(L) >= 6) setPageRows(luaL_checkinteger(L, 6));

    for (int i = 0; i < positions.size(); ++i) {
        positions[i].first = x + i * dx; positions[i].second = y + i * dy;
    } return 0;
}

int ShadyLua::MenuCursorProxy::getPosition(lua_State* L) {
    int index = luaL_checkinteger(L, 2);
    if (index < 0 || index >= positions.size()) return luaL_argerror(L, 2, "index out of bounds");
    auto& entry = positions.at(index);
    lua_pushinteger(L, entry.first);
    lua_pushinteger(L, entry.second);
    return 2;
}

void ShadyLua::MenuCursorProxy::setPosition(int i, int x, int y) {
    if (i < 0 || i >= positions.size()) throw std::exception("index out of range");
    positions[i].first = x; positions[i].second = y;
}

void ShadyLua::Renderer::update() {
    for (auto& cursor : cursors) {
        if (!cursor.active) continue;
        if (cursor.updateProxy() && cursor.sfxId >= 0)
            SokuLib::playSEWaveBuffer(cursor.sfxId);
    }
    effects.Update();
}

void ShadyLua::Renderer::render() {
    if (!isActive) return;

    for(auto& cursor : cursors) {
        if (cursor.visible) cursor.render();
    }

    if (guiSchema.objects.size()) guiSchema.render4();
    for (auto i : activeLayers) {
        effects.Render(i);
        auto range = sprites.equal_range(i);
        for (auto iter = range.first; iter != range.second; ++iter) {
            if (iter->second.isEnabled) iter->second.render(0, 0); // TODO check position
        }
        if (i == 0 && ShadyLua::MenuProxy::kind())
            ShadyLua::MenuProxy::renderBox();
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
    if (!texture) Logger::Warning("Texture not found: ", name);
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
    int texture; SokuLib::textureMgr.createTextTexture(&texture, str, *font->handle, w, h, &outW, &outH);
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
    Effect* effect = (Effect*) effects.CreateEffect(id, x, y, dir, layer, 0);
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
    max = max(max, 1);
    int pos = (argc < 4) ? 0 : luaL_checkinteger(L, 4);
    auto& cursor = cursors.emplace_back(width, HORZ, max, pos);
    cursor.sfxId = 0x27;//default switch se
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
            auto effect = Stack<Effect*>::get(L, i);
            effect->unknown158 = false;
        } else if (Stack<MenuCursorProxy*>::Helper::isInstance(L, i)) {
            if (cursors.empty()) continue;
            auto cursor = Stack<MenuCursorProxy*>::get(L, i);
            // just hide it, unless it is on ends
            if (&cursors.back() == cursor) cursors.pop_back();
            else if (&cursors.front() == cursor) cursors.pop_front();
            else cursor->active = cursor->visible = false;
        }
    }
    return 0;
}

void ShadyLua::Renderer::clear() {
    cursors.clear();
    guiSchema.clear();
    sprites.clear();
    effects.ClearPattern();
    activeLayers.clear();
}

static int font_loadFontFile(lua_State* L) {
    auto readFile = luabridge::getGlobal(L, "readfile");
    const char* filepath = luaL_checkstring(L, 1);
    auto data = readFile(filepath).cast<std::string>();
    DWORD numFonts = 0;
    AddFontMemResourceEx(data.data(), data.size(), 0, &numFonts);
    luabridge::push(L, numFonts);
    return 1;
}

ShadyLua::MenuProxy::MenuProxy(int handler, lua_State* L)
    : processHandler(handler), data(newTable(L)), script(ShadyLua::ScriptMap[L]) {}

void ShadyLua::MenuProxy::_() {}
int ShadyLua::MenuProxy::onProcess() {
    if (!ShadyLua::ScriptMap.contains(script->L))  return 0;
    int ret = 1;
    switch (kind()) {
    case 1:
        state = updateMsgBox();
        choice = false;
        break;
    case 2: case 3:
        int result = updateChoiceBox();
        state = result>2 ? 1 : result;
        choice = result==3;
        break;
    }
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
        } else if (lua_isboolean(L, -1)) {
            ret = lua_toboolean(L, -1);
        } lua_pop(L, 1);
    }
    return ret;
}

int ShadyLua::MenuProxy::onRender() {
    renderer.render();
    //if (kind()) renderBox();
    return 1;
}

bool ShadyLua::MenuProxy::ShowMessage(const char* text) {
    bool old = BoxSprite && BoxSprite->active;
    reinterpret_cast<void(__stdcall*)(const char*)>(0x443900)(text);
    renderer.activeLayers.insert(0);
    return old;
}

bool ShadyLua::MenuProxy::ShowChoice(const char* text, bool defaultYes) {
    bool old = BoxSprite && BoxSprite->active;
    reinterpret_cast<void(__stdcall*)(const char*, bool)>(0x443730)(text, defaultYes);
    renderer.activeLayers.insert(0);
    return old;
}

bool ShadyLua::MenuProxy::RemoveMessage() {
    bool old = BoxSprite && BoxSprite->active;
    reinterpret_cast<void(__stdcall*)(void)>(0x443730)();
    return old;
}

int ShadyLua::MenuProxy::updateMsgBox() {
    return reinterpret_cast<int(__stdcall*)(void)>(0x443960)();
}

int ShadyLua::MenuProxy::updateChoiceBox() {
    return reinterpret_cast<int(__stdcall*)(void)>(0x4437d0)();
}

void ShadyLua::MenuProxy::renderBox() {
    return reinterpret_cast<void(__stdcall*)(void)>(0x4439f0)();
}

int ShadyLua::EffectManagerProxy::loadPattern(lua_State* L) {
    const int argc = lua_gettop(L);
    const char* name = luaL_checkstring(L, 2);
    int reserve = argc < 3 ? 0 : luaL_checkinteger(L, 3);
    LoadPattern(name, reserve);
    return 0;
}

std::unordered_multimap<SokuLib::IScene*, ShadyLua::SceneProxy*> ShadyLua::SceneProxy::listeners;
ShadyLua::SceneProxy::SceneProxy(lua_State* L)
    : processHandler(LUA_REFNIL), data(newTable(L)), script(ShadyLua::ScriptMap[L]) {}
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
        } else if (lua_isboolean(L, -1)) {
            auto ret = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return ret;
        } else lua_pop(L, 1);
    }
    return -1;
}

ShadyLua::FontProxy::FontProxy() {
    weight = 400; height = 14;
    italic = shadow = useOffset = false;
    offsetX = offsetY = charSpaceX = charSpaceY = 0;
    bufferSize = 100000;
    r1 = g1 = b1 = b2 = r2 = g2 = 0xff;
    strcpy_s(faceName, SokuLib::defaultFontName);
}

template <typename T> static inline T& castFromPtr(size_t addr) { return *(T*)addr; }

void ShadyLua::LualibGui(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("gui")
            .addCFunction("OpenMenu", gui_OpenMenu)
            .addFunction("isMenuOpen", gui_IsOpenMenu)
            .beginClass<ShadyLua::Renderer>("Renderer")
                .addData("design", &ShadyLua::Renderer::guiSchema, false)
                .addProperty("effects", gui_renderer_getEffects, 0)
                .addData("isActive", &ShadyLua::Renderer::isActive, true)

                .addFunction("createSprite", &ShadyLua::Renderer::createSprite)
                .addFunction("createText", &ShadyLua::Renderer::createText)
                .addFunction("createEffect", &ShadyLua::Renderer::createEffect)
                .addFunction("createCursorH", &ShadyLua::Renderer::createCursor<true>)
                .addFunction("createCursorV", &ShadyLua::Renderer::createCursor<false>)
                .addFunction("destroy", &ShadyLua::Renderer::destroy) // TODO test to set nil to things
            .endClass()
            .beginClass<ShadyLua::MenuProxy>("Menu")
                .addProperty("renderer", gui_menu_getRenderer, 0)
                .addProperty("input", gui_getInput, 0)
                .addData("data", &MenuProxy::data, true)

                .addData("msgState", &MenuProxy::state, false)
                .addData("choice", &MenuProxy::choice, false)
                .addFunction("ShowMessage", &MenuProxy::ShowMessage)
                .addFunction("ShowChoice", &MenuProxy::ShowChoice)
                .addFunction("RemoveMessage", &MenuProxy::RemoveMessage)
            .endClass()
            .beginClass<ShadyLua::SceneProxy>("Scene")
                .addProperty("renderer", gui_scene_getRenderer, 0)
                .addProperty("input", gui_getInput, 0)
                .addData("data", &SceneProxy::data, true)
            .endClass()
            .beginClass<SokuLib::CDesign>("Design")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::CDesign>)
                .addConstructor<void(*)(void), RefCountedPtr<SokuLib::CDesign> >()
                .addFunction("setColor", &SokuLib::CDesign::setColor)
                .addFunction("loadResource", &SokuLib::CDesign::loadResource)
                .addFunction("clear", &SokuLib::CDesign::clear)
                .addFunction("getItemById", gui_design_GetItemById)
                .addFunction("getItem", gui_design_GetItem)
                .addFunction("getItemCount", gui_design_GetItemCount)
            .endClass()
            .beginClass<SokuLib::CDesign::Object>("DesignObject")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::CDesign::Object>)
                .addData("x", &SokuLib::CDesign::Object::x2, true)
                .addData("y", &SokuLib::CDesign::Object::y2, true)
                .addData("isActive", &SokuLib::CDesign::Object::active, true)
                .addFunction("setColor", &SokuLib::CDesign::Object::setColor)
                .addFunction("getValueControl", gui_design_getValue)
            .endClass()
            .beginClass<ValueProxy>("DesignValue")
                .addData("gauge", &ValueProxy::gauge, true)
                .addData("number", &ValueProxy::number, true)
            .endClass()
            .beginClass<SokuLib::KeyInputLight>("KeyInputLight")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::KeyInputLight>)
                .addData("axisH", &SokuLib::KeyInputLight::horizontalAxis, false)
                .addData("axisV", &SokuLib::KeyInputLight::verticalAxis, false)
                .addData("a", &SokuLib::KeyInputLight::a, false)
                .addData("b", &SokuLib::KeyInputLight::b, false)
                .addData("c", &SokuLib::KeyInputLight::c, false)
                .addData("d", &SokuLib::KeyInputLight::d, false)
                .addData("change", &SokuLib::KeyInputLight::changeCard, false)
                .addData("spell", &SokuLib::KeyInputLight::spellcard, false)
            .endClass()
            .deriveClass<SokuLib::KeyInput, SokuLib::KeyInputLight>("KeyInput")
                .addStaticFunction("fromPtr", castFromPtr<SokuLib::KeyInput>)
                .addData("pause", &SokuLib::KeyInput::pause, false)
                .addData("enter", &SokuLib::KeyInput::select, false)
            .endClass()
            .beginClass<ShadyLua::SpriteProxy>("Sprite")
                .addData("isEnabled", &ShadyLua::SpriteProxy::isEnabled, true)
                .addData("position", &ShadyLua::SpriteProxy::pos, true)
                .addData("scale", &ShadyLua::SpriteProxy::scale, true)
                .addData("rotation", &ShadyLua::SpriteProxy::rotation, true)
                .addFunction("setColor", &ShadyLua::SpriteProxy::setColor)
                .addFunction("setTexture", &ShadyLua::SpriteProxy::setTexture3)
                .addFunction("setText", &ShadyLua::SpriteProxy::setText)
            .endClass()
            .beginClass<ShadyLua::MenuCursorProxy>("Cursor")
                .addData("isActive", &MenuCursorProxy::active, true)
                .addData("isVisible", &MenuCursorProxy::visible, true)
                .addData("max", &MenuCursorProxy::max, false)
                .addProperty("index", &ShadyLua::MenuCursorProxy::getIndex, &ShadyLua::MenuCursorProxy::setIndex)
                .addData("page", &MenuCursorProxy::pgPos, false)
                .addData("sfxId", &MenuCursorProxy::sfxId, true)//negative number to disable
                .addFunction("getPosition", &MenuCursorProxy::getPosition)
                .addFunction("setPosition", &MenuCursorProxy::setPosition)
                .addFunction("setPageRows", &MenuCursorProxy::setPageRows)//zero to disable
                .addFunction("setRange", &MenuCursorProxy::setRange)
                .addFunction("pgUp", &MenuCursorProxy::pgUp)
                .addFunction("pgDn", &MenuCursorProxy::pgDn)
            .endClass()
            .beginClass<ShadyLua::EffectManagerProxy>("EffectManager")
                .addFunction("loadResource", &ShadyLua::EffectManagerProxy::loadPattern)
                .addFunction("clear", &ShadyLua::EffectManagerProxy::ClearPattern)
                .addFunction("clearEffects", &ShadyLua::EffectManagerProxy::ClearEffects)
            .endClass()
            .beginClass<ShadyLua::Renderer::Effect>("Effect")
                .addData("isEnabled", &ShadyLua::Renderer::Effect::isActive, true)
                .addData("isAlive", &ShadyLua::Renderer::Effect::unknown158, true)
                .addData("position", &ShadyLua::Renderer::Effect::position, true)
                .addData("speed", &ShadyLua::Renderer::Effect::speed, true)
                .addData("gravity", &ShadyLua::Renderer::Effect::gravity, true)
                .addData("center", &ShadyLua::Renderer::Effect::center, true)
                // TODO render options
                .addFunction("setActionSequence", &ShadyLua::Renderer::Effect::setActionSequence)
                .addFunction("setAction", &ShadyLua::Renderer::Effect::setAction)
                .addFunction("setSequence", &ShadyLua::Renderer::Effect::setSequence)
                .addFunction("resetSequence", &ShadyLua::Renderer::Effect::resetSequence)
                .addFunction("prevSequence", &ShadyLua::Renderer::Effect::prevSequence)
                .addFunction("nextSequence", &ShadyLua::Renderer::Effect::nextSequence)
                .addFunction("setPose", &ShadyLua::Renderer::Effect::setPose)
                .addFunction("prevPose", &ShadyLua::Renderer::Effect::prevPose)
                .addFunction("nextPose", &ShadyLua::Renderer::Effect::nextPose)
            .endClass()
            .beginClass<ShadyLua::FontProxy>("Font")
                .addConstructor<void(*)(void), RefCountedPtr<ShadyLua::FontProxy>>()
                .addFunction("setFontName", &ShadyLua::FontProxy::setFontName)
                .addFunction("setColor", &ShadyLua::FontProxy::setColor)
                .addProperty("height", &ShadyLua::FontProxy::height, true)
                .addProperty("weight", &ShadyLua::FontProxy::weight, true)
                .addProperty("italic", &ShadyLua::FontProxy::italic, true)
                .addProperty("shadow", &ShadyLua::FontProxy::shadow, true)
                .addProperty("wrap", &ShadyLua::FontProxy::useOffset, true)
                .addProperty("offsetX", &ShadyLua::FontProxy::offsetX, true)
                .addProperty("offsetY", &ShadyLua::FontProxy::offsetY, true)
                .addProperty("spacingX", &ShadyLua::FontProxy::charSpaceX, true)
                .addProperty("spacingY", &ShadyLua::FontProxy::charSpaceY, true)
                .addStaticFunction("loadFontFile", font_loadFontFile)
            .endClass()
        .endNamespace()
    ;
}