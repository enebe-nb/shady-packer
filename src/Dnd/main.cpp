#include "../Core/package.hpp"

namespace ShadyCore {
    void* Allocate(size_t s) { return new uint8_t[s]; }
    void Deallocate(void* p) { delete p; }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Too few arguments, drop your files in the executable.");
        return 1;
    }

    ShadyCore::PackageEx basePackage(std::filesystem::current_path());
    for (int i = 1; i < argc; ++i) {
        std::filesystem::path filename(argv[i]);
        if (std::filesystem::is_directory(filename) || filename.extension() == L".zip") {
            ShadyCore::Package group(filename);
            group.save(filename.replace_extension(L".dat"), ShadyCore::Package::DATA_MODE);
        } else {
            basePackage.insert(filename);
        }
    }

    if (basePackage.size()) {
        basePackage.save(std::filesystem::current_path()/L"my-new-pack.dat", ShadyCore::Package::DATA_MODE);
    }

    return 0;
}
