#pragma once

#include "../Core/baseentry.hpp"
#include <shared_mutex>

namespace ShadyCore {
    typedef struct EntryReader{
        BasePackageEntry& entry;
        std::istream& data;
        inline EntryReader(BasePackageEntry& entry)
            : entry(entry), data(entry.open()) {}
        inline ~EntryReader() { entry.close(data); }
    } EntryReader;

    extern const ptrdiff_t entry_reader_vtbl;
    extern const ptrdiff_t stream_reader_vtbl;
}