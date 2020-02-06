#pragma once

#include <SokuLib.h>
#include "../Core/baseentry.hpp"

void setup_entry_reader(SokuData::FileLoaderData& data, ShadyCore::BasePackageEntry& entry);
void setup_buffer_reader(SokuData::FileLoaderData& data, char* buffer, size_t size);
void setup_stream_reader(SokuData::FileLoaderData& data, std::istream* stream, size_t size);