#pragma once
#include <cstdint>
#include <cstring>
#include <istream>
#include <map>

namespace ShadyUtil {
    class RiffDocument {
    private:
        struct entryCompare { inline bool operator() (const char* left, const char* right) const { return strcmp(left, right) < 0; } };
        typedef struct { uint32_t offset; uint32_t size; } DataType;
		typedef std::map<const char *, DataType, entryCompare> MapType;
        MapType chunks;

        std::istream& input;
    public:
        void read(const char*, uint8_t*, uint32_t = 0, uint32_t = 0);

        RiffDocument(std::istream&);
        inline ~RiffDocument() { for (auto chunk : chunks) delete[] chunk.first; }
        inline uint32_t size(const char* key) { auto i = chunks.find(key); return i != chunks.end() ? i->second.size : 0; }
    };
}
