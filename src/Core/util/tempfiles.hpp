#pragma once

#include <filesystem>
#ifdef _WIN32
	#include <windows.h>
#else
	#include <stdlib.h>
#endif

// Required to bypass bug on MSYS
namespace ShadyUtil {
    inline const std::filesystem::path& TempDir() {
        static std::filesystem::path p;
        if (p.empty()) {
#ifdef _WIN32
            std::wstring buffer;
            size_t len = MAX_PATH + 1;
            do { // allow long paths for windows 10
                buffer.resize(len);
                len = GetTempPathW(buffer.size(), buffer.data());
            } while (len > buffer.size());
            if (len == 0) throw "Failure while finding the temp dir";

            buffer.resize(len);
            p = buffer;
#else
            const char* tmpdir = nullptr;
            const char* env[] = { "TMPDIR", "TMP", "TEMP", "TEMPDIR", nullptr };
            for (auto e = env; tmpdir == nullptr && *e != nullptr; ++e)
                tmpdir = ::getenv(*e);
            p = tmpdir ? tmpdir : "/tmp";
#endif
        }
        return p;
    }

    inline std::filesystem::path TempFile(const std::filesystem::path& base = TempDir()) {
#ifdef _WIN32
        return base / _wtempnam(base.c_str(), L"shady-");
#else
        return base / tempnam(base.c_str(), "shady-");
#endif
    }
}
