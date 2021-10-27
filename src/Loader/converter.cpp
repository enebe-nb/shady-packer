#include "main.hpp"

#include <sstream>
#include <SokuLib.hpp>
#include <curl/curl.h>

#include "../Core/package.hpp"
#include "../Core/reader.hpp"
#include "modpackage.hpp"
#include "menu.hpp"
#include "../Lua/lualibs/soku.hpp"

ShadyCore::PackageEx package; // TODO limit access and fix lua
namespace {
    auto __readerCreate = reinterpret_cast<void* (__stdcall *)(const char *filename, unsigned int *size, unsigned int *offset)>(0x0041c080);
    auto __loader = reinterpret_cast<int(*)()>(0);
    bool _initialized = false;

    int (SokuLib::Title::* __titleOnProcess)();
    int (SokuLib::Title::* __titleOnRender)();

    struct {
        SokuLib::Sprite* sprite = 0;
        int timeout = 240;
        bool shown = false;
    } menuHint;
}

static void* __stdcall readerCreate(const char *filename, unsigned int *_size, unsigned int *_offset) {
    int esi_value;
	__asm mov esi_value, esi

    loadLock.lock_shared();
    auto iter = package.find(filename);

    void* result;
    if (iter != package.end()) {
        auto type = iter.fileType();
        *_offset = 0x40000000; // just to hold a value
        if (type.format <= 0 || type.format >= 4) { // || type == ShadyCore::FileType::TYPE_UNKNOWN
            *_size = iter->second->getSize();
            *(int*)esi_value = ShadyCore::entry_reader_vtbl;
            result = new ShadyCore::EntryReader(*iter->second, loadLock);
        } else {
            // TODO this convertion is slow, change it
            std::istream& input = iter->second->open();
            std::stringstream* buffer = new std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
            ShadyCore::convertResource(type.type, type.format, input, (ShadyCore::FileType::Format)0, *buffer); // TODO fix
            *_size = buffer->tellp();
            iter->second->close();

            *(int*)esi_value = ShadyCore::stream_reader_vtbl;
            result = buffer;
        }
    } else {
        // result = __readerCreate(filename, _size, _offset);
        __asm {
            push _offset;
            push _size;
            push filename;
	        mov esi, esi_value;
            call __readerCreate;
            mov result, eax;
        }
    }
    loadLock.unlock_shared();

    return result;
}

static int __fastcall titleOnProcess(SokuLib::Title* t) {
    int ret = (t->*__titleOnProcess)();

    if (!menuHint.sprite && !menuHint.shown) {
        menuHint.shown = true;

        int texture = 0;
        SokuLib::SWRFont font; font.create();
        font.setIndirect(SokuLib::FontDescription{
            "Courier New",
            255,255,255,255,255,255,
            14, 500,
            false, true, false,
            0, 0, 0, 0, 0
        });
        SokuLib::textureMgr.createTextTexture(&texture, "Enabled shady-loader. Press F2 to open the menu.", font, 640, 20, 0, 0);
        font.destruct();

        menuHint.sprite = new SokuLib::Sprite();
        menuHint.sprite->setTexture2(texture, 0, 0, 640, 20);
    } else if (menuHint.sprite && menuHint.timeout <= 0) {
        SokuLib::textureMgr.remove(menuHint.sprite->dxHandle);
        delete menuHint.sprite;
        menuHint.sprite = 0;
    }

    if (SokuLib::checkKeyOneshot(0x3C, false, false, false) && !*((int*)0x89a88c)) {
        // the virtual destructor will call free from this binary
        // so here should be `new` instead of `SokuLib::New`
        SokuLib::activateMenu(new ModMenu());
    }

    return ret;
}

static int __fastcall titleOnRender(SokuLib::Title* t) {
    int ret = (t->*__titleOnRender)();

    if (menuHint.sprite && menuHint.timeout > 0) {
        if (menuHint.timeout < 20) menuHint.sprite->setColor(((0xff*menuHint.timeout/20) << 24)|0xffffff);
        menuHint.sprite->render(0, 0);
        --menuHint.timeout;
    }

    return ret;
}

static int _HookLoader() {
    if (__loader) __loader();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    package.merge(std::filesystem::relative(ModPackage::basePath / L"shady-loader.dat"));

    DWORD dwOldProtect;
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    __readerCreate = SokuLib::TamperNearJmpOpr(0x0040D227, readerCreate);
    VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    __titleOnProcess  = SokuLib::TamperDword(&SokuLib::VTable_Title.onProcess, titleOnProcess);
    __titleOnRender  = SokuLib::TamperDword(&SokuLib::VTable_Title.onRender, titleOnRender);
    VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, dwOldProtect, &dwOldProtect);

    if (iniAutoUpdate) ModPackage::LoadFromRemote();

    _initialized = true;
    // set EAX to restore hooked instruction
    return *(int*)0x008943b8;
}

void HookLoader(const std::wstring& caller) {
    ShadyLua::LoadTamper(caller);

    if (caller == L"SokuEngine.dll") _HookLoader();
    else {
        DWORD dwOldProtect;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
        bool hasCall = 0xE8 == *(unsigned char*)0x007fb596;
        __loader = SokuLib::TamperNearCall(0x007fb596, _HookLoader);
        if (!hasCall) __loader = 0;
        ::VirtualProtect(reinterpret_cast<LPVOID>(0x007fb596), 5, dwOldProtect, &dwOldProtect);
    }
}

void UnloadLoader() {
    ShadyLua::UnloadTamper();

    if (!_initialized) return;
    DWORD dwOldProtect;
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
    SokuLib::TamperNearJmpOpr(0x0040D227, __readerCreate);
    ::VirtualProtect(reinterpret_cast<LPVOID>(0x0040D227), 5, dwOldProtect, &dwOldProtect);

    // TODO join downloadController before this
    curl_global_cleanup();
}