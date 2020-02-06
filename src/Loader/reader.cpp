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

static int WINAPI reader_getSize() {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    if (data->offset > data->size) data->offset = 0;
    __asm mov ecx, ecx_value
    return data->size;
}

static int WINAPI reader_getRead() {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    __asm mov ecx, ecx_value
    return data->read;
}

static int WINAPI entry_reader_destruct(int a) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value+4);
    ((entry_pair*)data->data)->first->close();
    delete (entry_pair*) data->data;
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
    data->read = ((entry_pair*)data->data)->second->gcount();
    data->offset += data->read;
    __asm mov ecx, ecx_value
    return data->read > 0;
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
    data->offset = ((entry_pair*)data->data)->second->tellg();
    __asm mov ecx, ecx_value
}

static int WINAPI buffer_reader_destruct(int a) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value+4);
    delete[] (char*)data->data;
    if (a & 1) orig_dealloc(ecx_value);
    __asm mov ecx, ecx_value
    return ecx_value;
}

static int WINAPI buffer_reader_read(char *dest, int size) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (!ecx_value) return 0;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    if (data->offset + size <= data->size) data->read = size;
    else data->read = data->size - data->offset;
    memcpy(dest, (char*)data->data + data->offset, data->read);
    data->offset += data->read;
    __asm mov ecx, ecx_value
    return data->read > 0;
}

static void WINAPI buffer_reader_seek(int offset, int whence) {
    int ecx_value;
    __asm mov ecx_value, ecx
    if (ecx_value == 0) return;

    reader_data_t *data = (reader_data_t *)(ecx_value + 4);
    switch(whence) {
    case SEEK_SET:
        if (offset > data->size) data->offset = data->size;
        else data->offset = offset;
        break;
    case SEEK_CUR:
        if (data->offset + offset > data->size) data->offset = data->size;
        else if (data->offset + offset < 0) data->offset = 0;
        else data->offset += offset;
        break;
    case SEEK_END:
        if (offset > data->size) data->offset = 0;
        else data->offset = data->size - offset;
        break;
    }
    __asm mov ecx, ecx_value
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
    reader_getRead,
    entry_reader_seek,
    reader_getSize
};

functable buffer_reader_table = {
    buffer_reader_destruct,
    buffer_reader_read,
    reader_getRead,
    buffer_reader_seek,
    reader_getSize
};

functable stream_reader_table = {
    stream_reader_destruct,
    stream_reader_read,
    reader_getRead,
    stream_reader_seek,
    reader_getSize
};

void setup_entry_reader(SokuData::FileLoaderData& data, ShadyCore::BasePackageEntry& entry) {
    data.data = new entry_pair(&entry, &entry.open());
    data.size = entry.getSize();
    data.reader = &entry_reader_table;
}

void setup_buffer_reader(SokuData::FileLoaderData& data, char* buffer, size_t size) {
    data.data = buffer;
    data.size = size;
    data.reader = &buffer_reader_table;
}

void setup_stream_reader(SokuData::FileLoaderData& data, std::istream* stream, size_t size) {
    data.data = stream;
    data.size = size;
    data.reader = &stream_reader_table;
}