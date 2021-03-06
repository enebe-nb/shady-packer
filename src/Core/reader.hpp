#pragma once

#include "../Core/baseentry.hpp"
#include <shared_mutex>

namespace ShadyCore {
    typedef struct EntryReader{
        BasePackageEntry& entry;
        std::istream& data;
        std::shared_mutex& mutex;
        inline EntryReader(BasePackageEntry& entry, std::shared_mutex& mutex)
            : entry(entry), data(entry.open()), mutex(mutex) {mutex.lock_shared();}

        inline ~EntryReader() {entry.close(); mutex.unlock_shared();}
    } EntryReader;
    extern const int entry_reader_vtbl;
    extern const int stream_reader_vtbl;
}