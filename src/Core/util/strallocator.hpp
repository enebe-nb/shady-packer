#pragma once
#include <vector>

namespace ShadyUtil {
    class StrAllocator {
    private:
        std::vector<char*> strings;
    public:
        inline StrAllocator(unsigned int reserve = 100) { strings.reserve(reserve); }
        virtual ~StrAllocator();
        char* allocateString(const char*, unsigned int = 0);
        char* allocateString(unsigned int);
		virtual void clear();
    };
}
