#include <gtest/gtest.h>
#include <benchmark/benchmark.h>

namespace ShadyCore {
    void* Allocate(size_t s) { return new uint8_t[s]; }
    void Deallocate(void* p) { delete p; }
}

int main(int argc, char **argv) {
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    testing::InitGoogleTest(&argc, argv);
    return testing::UnitTest::GetInstance()->Run();
}