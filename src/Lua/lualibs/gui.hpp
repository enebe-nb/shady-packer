#include "../lualibs.hpp"
#include <LuaBridge/LuaBridge.h>
#include <SokuLib.hpp>
#include <deque>
#include <set>
#include <array>

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

    template<class T, std::size_t N> class ArrayRef : public std::array<T, N> {};
    template <typename T, size_t N> static ShadyLua::ArrayRef<T, N>* ArrayRef_from(T(*ptr)[N]) { return (ShadyLua::ArrayRef<T, N>*)(ptr); }
    template <typename T, class C, size_t N> static ShadyLua::ArrayRef<T, N> C::* ArrayRef_from(T(C::*ptr)[N]) { return (ShadyLua::ArrayRef<T, N> C::*)(ptr); }
}

namespace luabridge {

template<class T, std::size_t N>
struct Stack<ShadyLua::ArrayRef<T, N>> {
    static int __index(lua_State* L) {
        if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected a table element.");
        int index = luaL_checkinteger(L, 2);
        if (index <= 0 || index > N) return luaL_argerror(L, 2, "index out of bounds.");

        lua_getmetatable(L, 1);
        lua_pushstring(L, "__addr");
        lua_rawget(L, -2);
        auto& self = *(ShadyLua::ArrayRef<T, N>*)(lua_topointer(L, -1));
        lua_pop(L, 2);

        Stack<T>::push(L, self[index]);
        return 1;
    }

    static int __newindex(lua_State* L) {
        if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected a table element.");
        int index = luaL_checkinteger(L, 2);
        if (index <= 0 || index > N) return luaL_argerror(L, 2, "index out of bounds.");
        if (!Stack<T>::isInstance(L, 3)) return luaL_argerror(L, 3, "array can't accept this variable type.");

        lua_getmetatable(L, 1);
        lua_pushstring(L, "__addr");
        lua_rawget(L, -2);
        auto& self = *(ShadyLua::ArrayRef<T, N>*)(lua_topointer(L, -1));
        lua_pop(L, 2);

        self[index] = Stack<T>::get(L, 3);
        return 0;
    }

    static int __len(lua_State* L) {
        lua_pushinteger(L, N);
        return 1;
    }

    static void push(lua_State* L, ShadyLua::ArrayRef<T, N> const& array) {
        lua_newtable(L);
        lua_createtable(L, 0, 5);
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -3);

        lua_pushstring(L, "__addr");
        lua_pushlightuserdata(L, (void*)&array);
        lua_rawset(L, -3);
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, &__index);
        lua_rawset(L, -3);
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, &__newindex);
        lua_rawset(L, -3);
        lua_pushstring(L, "__len");
        lua_pushcfunction(L, &__len);
        lua_rawset(L, -3);
        lua_pushstring(L, "__metatable");
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pop(L, 1);
    }

    static ShadyLua::ArrayRef<T, N> get(lua_State* L, int index) {
        if (!lua_istable(L, index)) luaL_error(L, "#%d argument must be table", index);
        if (get_length(L, index) != N) luaL_error(L, "array size must be %d ", N);

        ShadyLua::ArrayRef<T, N> array;
        int const absindex = lua_absindex(L, index);
        lua_pushnil(L);
        int arrayIndex = 0;
        while (lua_next(L, absindex) != 0) {
            array[arrayIndex] = Stack<T>::get(L, -1);
            lua_pop(L, 1);
            ++arrayIndex;
        }
        return array;
    }

    static bool isInstance(lua_State* L, int index) {
        return lua_istable(L, index) && get_length(L, index) == N;
    }
};

} // namespace luabridge
