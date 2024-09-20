#include "../lualibs.hpp"
#include <LuaBridge/LuaBridge.h>
#include <SokuLib.hpp>
#include <deque>
#include <set>

namespace ShadyLua {
    class SpriteProxy : public SokuLib::Sprite {
    public:
        bool isEnabled = true;
        int setTexture3(lua_State* L);
        int setText(lua_State*);

        virtual ~SpriteProxy();
    };

    class MenuCursorProxy : public SokuLib::MenuCursor {
    public:
        int width; bool active = true;
        std::vector<std::pair<int,int>> positions;

        MenuCursorProxy(int w, bool horz, int max = 1, int pos = 0);
        void setPosition(int i, int x, int y);
        void setRange(int x, int y, int dx = 0, int dy = 0);
    };

    class EffectManagerProxy : public SokuLib::v2::EffectManager_Select {
    public:
        int loadPattern(lua_State* L);
    };

    class Renderer {
    public:
        std::deque<MenuCursorProxy> cursors;
        SokuLib::CDesign guiSchema;
        std::multimap<int, SpriteProxy> sprites;
        EffectManagerProxy effects;
        std::set<char> activeLayers;
        bool isActive = true;

        using Effect = SokuLib::v2::SelectEffectObject;
        ~Renderer() {guiSchema.clear();}
        void update();
        void render();

        int createSprite(lua_State* L);
        int createText(lua_State* L);
        int createEffect(lua_State* L);
        template<bool> int createCursor(lua_State* L);
        int destroy(lua_State* L);
        void clear();
    };

    class MenuProxy : public SokuLib::IMenu {
    private:
        const int processHandler;

    public:
        ShadyLua::LuaScript* const script;
        luabridge::LuaRef data;
        Renderer renderer;

        MenuProxy(int handler, lua_State* L);
        ~MenuProxy() override = default;
        void _() override;
        int onProcess() override;
        int onRender() override;

        bool ShowMessage(const char* text);
    };

    class SceneProxy {
    public:
        int processHandler;
        ShadyLua::LuaScript* script = 0;
        luabridge::LuaRef data;
        Renderer renderer;

        static std::unordered_multimap<SokuLib::IScene*, SceneProxy*> listeners;

        SceneProxy(lua_State* L);
        int onProcess();
    };

    class FontProxy : public SokuLib::FontDescription {
    public:
        SokuLib::SWRFont* handle = 0;

        FontProxy();
        inline ~FontProxy() { if (handle) { handle->destruct(); delete handle; } handle = 0; }
        inline void setFontName(const char* name) { strcpy(faceName, name); }
        inline void setColor(int c1, int c2) { r1 = c1 >> 16; g1 = c1 >> 8; b1 = c1; r2 = c2 >> 16; g2 = c2 >> 8; b2 = c2; }
        inline void prepare() { if (!handle) { handle = new SokuLib::SWRFont(); handle->create(); }  handle->setIndirect(*this); }
    };
}