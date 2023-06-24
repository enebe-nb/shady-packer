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

    extern const int entry_reader_vtbl;
    extern const int stream_reader_vtbl;
}