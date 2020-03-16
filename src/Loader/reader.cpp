#include "reader.hpp"

namespace {
    typedef struct {
        void* data;
        int offset;
        int size;
        int read;
    } reader_data_t;

    typedef std::pair<ShadyCore::BasePackageEntry*, std::istream*> entry_pair;
    typedef void (WINAPI* orig_dealloc_t)(int);
    orig_dealloc_t orig_dealloc = (orig_dealloc_t)0x81f6fa;
}

static int WINAPI entry_reader_getSize() {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    if (data->data == (void*)data->offset) data->offset = 0;
    int size = ((entry_pair*)data->data)->first->getSize();
    __asm mov ecx, ecx_value
    return size;
}

static int WINAPI entry_reader_getRead() {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    int read = ((entry_pair*)data->data)->second->gcount();
    __asm mov ecx, ecx_value
    return read;
}

static int WINAPI entry_reader_destruct(int a) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value+4);
    ((entry_pair*)data->data)->first->close();
    //delete (entry_pair*) data->data; // crash?????
    if (a & 1) orig_dealloc(ecx_value);
    __asm mov ecx, ecx_value
    return ecx_value;
}

static int WINAPI entry_reader_read(char *dest, int size) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    ((entry_pair*)data->data)->second->read(dest, size);
    __asm mov ecx, ecx_value
    return !((entry_pair*)data->data)->second->eof();
}

static void WINAPI entry_reader_seek(int offset, int whence) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    switch(whence) {
    case SEEK_SET:
        ((entry_pair*)data->data)->second->seekg(offset, std::ios::beg);
        break;
    case SEEK_CUR:
        ((entry_pair*)data->data)->second->seekg(offset, std::ios::cur);
        break;
    case SEEK_END:
        ((entry_pair*)data->data)->second->seekg(-offset, std::ios::end);
        break;
    }
    if (((entry_pair*)data->data)->second->fail()) ((entry_pair*)data->data)->second->clear();
    __asm mov ecx, ecx_value
}

static int WINAPI stream_reader_getSize() {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    __asm mov ecx, ecx_value
    return data->size;
}

static int WINAPI stream_reader_getRead() {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    __asm mov ecx, ecx_value
    return data->read;
}

static int WINAPI stream_reader_destruct(int a) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value+4);
    delete (std::istream*)data->data;
    if (a & 1) orig_dealloc(ecx_value);
    __asm mov ecx, ecx_value
    return ecx_value;
}

static int WINAPI stream_reader_read(char *dest, int size) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    ((std::istream*)data->data)->read(dest, size);
    data->read = ((std::istream*)data->data)->gcount();
    data->offset += data->read;
    __asm mov ecx, ecx_value
    return data->read > 0;
}

static void WINAPI stream_reader_seek(int offset, int whence) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    switch(whence) {
    case SEEK_SET:
        ((std::istream*)data->data)->seekg(offset, std::ios::beg);
        break;
    case SEEK_CUR:
        ((std::istream*)data->data)->seekg(offset, std::ios::cur);
        break;
    case SEEK_END:
        ((std::istream*)data->data)->seekg(-offset, std::ios::end);
        break;
    }
    data->offset = ((std::istream*)data->data)->tellg();
    __asm mov ecx, ecx_value
}

functable entry_reader_table = {
    entry_reader_destruct,
    entry_reader_read,
    entry_reader_getRead,
    entry_reader_seek,
    entry_reader_getSize
};

functable stream_reader_table = {
    stream_reader_destruct,
    stream_reader_read,
    stream_reader_getRead,
    stream_reader_seek,
    stream_reader_getSize
};

void setup_entry_reader(SokuData::FileLoaderData& data, ShadyCore::BasePackageEntry& entry) {
    data.data = new entry_pair(&entry, &entry.open());
    data.size = entry.getSize();
    data.reader = &entry_reader_table;
}

void setup_stream_reader(SokuData::FileLoaderData& data, std::istream* stream, size_t size) {
    data.data = stream;
    data.size = size;
    data.reader = &stream_reader_table;
}