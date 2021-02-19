#pragma once

#include <SokuLib.h>
#include "../Core/baseentry.hpp"

void setup_entry_reader(SokuData::FileLoaderData& data, ShadyCore::BasePackageEntry& entry);
int setup_entry_reader(int esi, ShadyCore::BasePackageEntry& entry);
void setup_stream_reader(SokuData::FileLoaderData& data, std::istream* stream, size_t size);
int setup_stream_reader(int esi, std::istream* stream, size_t size);