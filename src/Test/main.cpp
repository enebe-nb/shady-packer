#include <gtest/gtest.h>
#include <benchmark/benchmark.h>

int main(int argc, char **argv) {
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    testing::InitGoogleTest(&argc, argv);
    return testing::UnitTest::GetInstance()->Run();
}