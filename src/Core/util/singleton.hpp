#pragma once

namespace ShadyUtil {
    template<class Target>
    class Singleton {
    private:
        static Target* self;
    protected:
        Singleton() {}
    public:
        virtual ~Singleton() { self = 0; }

        inline static Target* get() {
            if (!self) self = new Target;
            return self;
        }
    };

    // Inheritance static check and initialization to null.
    template<class Target> Target* Singleton<Target>::self = static_cast<Target*>((Singleton<Target>*)0);
}
