#include <benchmark/benchmark.h>
#include "thread_pool.h"

#include <vector>
#include <future>
#include <cmath>
#include <algorithm>
#include <random>

// --------------- Single-threaded CPU-heavy baseline -----------------

static void BM_SingleThread_CPUHeavy(benchmark::State& state) {
    int iters = static_cast<int>(state.range(0));
    for (auto _ : state) {
        double acc = 0.0;
        for (int i = 0; i < iters; ++i) {
            acc += std::sin(i * 0.001) * std::cos(i * 0.002);
        }
        benchmark::DoNotOptimize(acc);
    }
}

BENCHMARK(BM_SingleThread_CPUHeavy)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000);

// --------------- ThreadPool CPU-heavy benchmark -----------------

static void BM_ThreadPool_CPUHeavy(benchmark::State& state) {
    int itersPerTask = static_cast<int>(state.range(0));
    int numTasks = 1000;
    int numThreads = 4;

    for (auto _ : state) {
        ThreadPoolScheduler scheduler(numThreads);

        std::vector<std::future<void>> futures;
        futures.reserve(numTasks);

        for (int i = 0; i < numTasks; ++i) {
            futures.push_back(
                scheduler.submit([itersPerTask]() {
                    double acc = 0.0;
                    for (int j = 0; j < itersPerTask; ++j) {
                        acc += std::sin(j * 0.001) * std::cos(j * 0.002);
                    }
                    benchmark::DoNotOptimize(acc);
                })
            );
        }

        for (auto& f : futures) {
            f.get();
        }

        scheduler.shutdown();
    }
}

BENCHMARK(BM_ThreadPool_CPUHeavy)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000);

// --------------- Single-threaded Memory-heavy baseline -----------------

static void BM_SingleThread_MemoryHeavy(benchmark::State& state) {
    int size = static_cast<int>(state.range(0));

    for (auto _ : state) {
        std::vector<int> v(size);
        for (int i = 0; i < size; ++i) v[i] = i;

        static thread_local std::mt19937 rng{std::random_device{}()};
        std::shuffle(v.begin(), v.end(), rng);

        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_SingleThread_MemoryHeavy)
    ->Arg(10000)
    ->Arg(20000);

// --------------- ThreadPool Memory-heavy benchmark -----------------

static void BM_ThreadPool_MemoryHeavy(benchmark::State& state) {
    int size = static_cast<int>(state.range(0));
    int numTasks = 500;
    int numThreads = 4;

    for (auto _ : state) {
        ThreadPoolScheduler scheduler(numThreads);
        std::vector<std::future<void>> futures;
        futures.reserve(numTasks);

        for (int i = 0; i < numTasks; ++i) {
            futures.push_back(
                scheduler.submit([size]() {
                    std::vector<int> v(size);
                    for (int j = 0; j < size; ++j) v[j] = j;

                    static thread_local std::mt19937 rng{std::random_device{}()};
                    std::shuffle(v.begin(), v.end(), rng);

                    std::sort(v.begin(), v.end());
                    benchmark::DoNotOptimize(v);
                })
            );
        }

        for (auto& f : futures) {
            f.get();
        }

        scheduler.shutdown();
    }
}

BENCHMARK(BM_ThreadPool_MemoryHeavy)
    ->Arg(10000)
    ->Arg(20000);


BENCHMARK_MAIN();