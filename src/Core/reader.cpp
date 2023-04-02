#include "reader.hpp"
#include <fstream>

#ifdef __GNUC__
#define __fastcall
#define __cdecl
#endif

namespace {
    typedef struct {
        int vtable;
        void* data;
        int lastRead;
        int size;
        int begin;
        int offset;
        int decryptKey;
    } reader_data_t;

    typedef struct {
        reader_data_t* (__fastcall *destruct)(reader_data_t*, void*, int);
        bool (__fastcall *read)(reader_data_t*, void*, char*, int);
        int (__fastcall *getRead)(reader_data_t*);
        int (__fastcall *seek)(reader_data_t*, void*, int, int);
        int (__fastcall *getSize)(reader_data_t*);
    } reader_vtbl_t;

    typedef void (__cdecl* orig_dealloc_t)(reader_data_t*);
    orig_dealloc_t orig_dealloc = (orig_dealloc_t)0x81f6fa;
}

static int __fastcall reader_getSize(reader_data_t* ecx) {
    return ecx->size;
}

static int __fastcall reader_getRead(reader_data_t* ecx) {
    return ecx->lastRead;
}

static reader_data_t* __fastcall entry_reader_destruct(reader_data_t* ecx, void* edx, int a) {
    delete (ShadyCore::EntryReader*)ecx->data;
    if (a & 1) orig_dealloc(ecx);
    return ecx;
}

static bool __fastcall entry_reader_read(reader_data_t* ecx, void* edx, char *dest, int size) {
    ecx->lastRead = ((ShadyCore::EntryReader*)ecx->data)->data.read(dest, size).gcount();
    ecx->offset += ecx->lastRead;
    return ecx->lastRead > 0;
}

static int __fastcall entry_reader_seek(reader_data_t* ecx, void* edx, int offset, int whence) {
    if (((ShadyCore::EntryReader*)ecx->data)->data.eof()) {
        ((ShadyCore::EntryReader*)ecx->data)->data.clear();
    }
    switch(whence) {
    case SEEK_SET:
        ((ShadyCore::EntryReader*)ecx->data)->data.seekg(offset, std::ios::beg);
        break;
    case SEEK_CUR:
        ((ShadyCore::EntryReader*)ecx->data)->data.seekg(offset, std::ios::cur);
        break;
    case SEEK_END:
        ((ShadyCore::EntryReader*)ecx->data)->data.seekg(-offset, std::ios::end);
        break;
    default: return 0;
    }
    return ecx->offset = ((ShadyCore::EntryReader*)ecx->data)->data.tellg();
}

static reader_data_t* __fastcall stream_reader_destruct(reader_data_t* ecx, void* edx, int a) {
    delete (std::istream*)ecx->data;
    if (a & 1) orig_dealloc(ecx);
    return ecx;
}

static bool __fastcall stream_reader_read(reader_data_t* ecx, void* edx, char *dest, int size) {
    ecx->lastRead = ((std::istream*)ecx->data)->read(dest, size).gcount();
    ecx->offset += ecx->lastRead;
    return ecx->lastRead > 0;
}

static int __fastcall stream_reader_seek(reader_data_t* ecx, void* edx, int offset, int whence) {
    if (((std::istream*)ecx->data)->eof()) {
        ((std::istream*)ecx->data)->clear();
    }
    switch(whence) {
    case SEEK_SET:
        ((std::istream*)ecx->data)->seekg(offset, std::ios::beg);
        break;
    case SEEK_CUR:
        ((std::istream*)ecx->data)->seekg(offset, std::ios::cur);
        break;
    case SEEK_END:
        ((std::istream*)ecx->data)->seekg(-offset, std::ios::end);
        break;
    default: return 0;
    }
    return ecx->offset = ((std::istream*)ecx->data)->tellg();
}

namespace {
    reader_vtbl_t entry_reader_table = {
        entry_reader_destruct,
        entry_reader_read,
        reader_getRead,
        entry_reader_seek,
        reader_getSize,
    };

    reader_vtbl_t stream_reader_table = {
        stream_reader_destruct,
        stream_reader_read,
        reader_getRead,
        stream_reader_seek,
        reader_getSize,
    };
}

const ptrdiff_t ShadyCore::entry_reader_vtbl = (ptrdiff_t)&entry_reader_table;
const ptrdiff_t ShadyCore::stream_reader_vtbl = (ptrdiff_t)&stream_reader_table;
