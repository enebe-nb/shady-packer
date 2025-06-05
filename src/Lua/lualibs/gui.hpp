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
        int width; bool active = true, visible = true, triggered = false;
        std::vector<std::pair<int,int>> positions;
        int sfxId = -1;

        MenuCursorProxy(int w, bool horz, int max = 1, int pos = 0);
        int getPosition(lua_State* L);
        void setPosition(int i, int x, int y);
        void setPageRows(int rows);
        void _setRange(int x, int y, int dx, int dy);
        int setRange(lua_State* L);
        inline void render() { SokuLib::MenuCursor::render(positions[pos - pgPos].first, positions[pos - pgPos].second, width); }
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
        int createCursorMat(lua_State* L);
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

    class CustomDataProxy {//for battle.Object.customData
        void* addr = addr;
    public:
        inline CustomDataProxy(void* addr) : addr(addr) {}
        int __index(lua_State* L);
        int __newindex(lua_State* L);
        inline int __len(lua_State* L) {
            // to warn about the unsafety maybe we should forbid #length
            return luaL_error(L, "This object has unconfirmed length.");
        }
    };

    template <class T, std::size_t N> class ArrayRef : public std::array<T, N> {};
    template <typename T, size_t N> static ShadyLua::ArrayRef<T, N>* ArrayRef_from(T(*ptr)[N]){ return (ShadyLua::ArrayRef<T, N>*)(ptr); }
    template <typename T, class C, size_t N> static ShadyLua::ArrayRef<T, N> C::* ArrayRef_from(T(C::*ptr)[N]) { return (ShadyLua::ArrayRef<T, N> C::*)(ptr); }
}

namespace luabridge {

template<class T, std::size_t N>
struct Stack<ShadyLua::ArrayRef<T, N>> {
    static int __index(lua_State* L) {
        if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected a table element.");
        int index = luaL_checkinteger(L, 2);
        if (index == 0 || index > int(N) || index < -int(N)) {
            lua_pushnil(L);
            return 1;//luaL_argerror(L, 2, "index out of bounds.");
        }
        index = (index - int(index > 0) + N) % N + 1;

        lua_getmetatable(L, 1);
        lua_pushstring(L, "__addr");
        lua_rawget(L, -2);
        auto& self = *(ShadyLua::ArrayRef<T, N>*)(lua_topointer(L, -1));
        lua_pop(L, 2);

        Stack<T>::push(L, self[index-1]);
        return 1;
    }

    static int __newindex(lua_State* L) {
        if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected a table element.");
        int index = luaL_checkinteger(L, 2);
        if (index == 0 || index > int(N) || index < -int(N)) return luaL_argerror(L, 2, "index out of bounds.");
        index = (index - int(index > 0) + N) % N + 1;
        if (!Stack<T>::isInstance(L, 3)) return luaL_argerror(L, 3, "array can't accept this variable type.");

        lua_getmetatable(L, 1);
        lua_pushstring(L, "__addr");
        lua_rawget(L, -2);
        auto& self = *(ShadyLua::ArrayRef<T, N>*)(lua_topointer(L, -1));
        lua_pop(L, 2);

        self[index-1] = Stack<T>::get(L, 3);
        return 0;
    }

    static int __len(lua_State* L) {
        lua_pushinteger(L, N);
        return 1;
    }
    static int _next(lua_State* L) {//t, k
        if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected a table element.");
        int index = 0;
        if (lua_isinteger(L, 2))
            index = luaL_checkinteger(L, 2);
        ++index;
        if (index > N) {
            lua_pushnil(L);
            return 1;
        }
        else {
            lua_pushinteger(L, index);
            lua_pushinteger(L, index);
            lua_gettable(L, 1);
        }
        return 2;//k, v
    }
    static int __pairs(lua_State* L) {//t
        lua_pushcfunction(L, &_next);
        lua_pushvalue(L, 1);
        lua_pushnil(L);
        return 3;//next, t, k0
    }

    static void push(lua_State* L, ShadyLua::ArrayRef<T, N> const& array) {
        lua_newtable(L);
        lua_createtable(L, 0, 6);
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
        lua_pushstring(L, "__pairs");
        lua_pushcfunction(L, &__pairs);
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