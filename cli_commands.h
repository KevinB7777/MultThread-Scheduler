#pragma once

#include <iostream>
#include <chrono>
#include <future>
#include <vector>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>

#include "thread_pool.h"

// All functions are inline so this header can be included in multiple .cpp files safely.

inline void printHelp() {
    std::cout << "Commands:\n"
              << "  sum N        - schedule a task that computes sum 1..N\n"
              << "  sleep ms     - schedule a task that sleeps for ms milliseconds\n"
              << "  metrics      - show detailed scheduler metrics (and log to file)\n"
              << "  benchmark    - run heavy single-thread vs thread-pool benchmarks\n"
              << "  help         - show this help message\n"
              << "  exit/quit    - exit the scheduler shell\n";
}

inline void handleMetrics(ThreadPoolScheduler& scheduler) {
    std::cout << "=== Scheduler Metrics ===\n";
    std::cout << "Tasks submitted:      " << scheduler.tasksSubmitted() << "\n";
    std::cout << "Tasks completed:      " << scheduler.tasksCompleted() << "\n";
    std::cout << "Uptime:               " << scheduler.uptimeSeconds() << " s\n";
    std::cout << "Throughput:           " << scheduler.throughputTasksPerSecond() << " tasks/s\n";
    std::cout << "Avg wait time:        " << scheduler.avgWaitMs() << " ms\n";
    std::cout << "Avg service time:     " << scheduler.avgServiceMs() << " ms\n";
    std::cout << "Total wait time:      " << scheduler.totalWaitMs() << " ms\n";
    std::cout << "Total service time:   " << scheduler.totalServiceMs() << " ms\n";
    std::cout << "Max wait time:        " << scheduler.maxWaitMs() << " ms\n";
    std::cout << "Max service time:     " << scheduler.maxServiceMs() << " ms\n";

    // Also log metrics to a file
    std::ofstream ofs("metrics.log", std::ios::app);
    if (ofs) {
        ofs << "=== Metrics Snapshot ===\n";
        ofs << "Tasks submitted: " << scheduler.tasksSubmitted() << "\n";
        ofs << "Tasks completed: " << scheduler.tasksCompleted() << "\n";
        ofs << "Uptime (s):      " << scheduler.uptimeSeconds() << "\n";
        ofs << "Throughput (tasks/s): " << scheduler.throughputTasksPerSecond() << "\n";
        ofs << "Avg wait (ms):   " << scheduler.avgWaitMs() << "\n";
        ofs << "Avg service (ms):" << scheduler.avgServiceMs() << "\n";
        ofs << "Total wait (ms): " << scheduler.totalWaitMs() << "\n";
        ofs << "Total svc (ms):  " << scheduler.totalServiceMs() << "\n";
        ofs << "Max wait (ms):   " << scheduler.maxWaitMs() << "\n";
        ofs << "Max svc (ms):    " << scheduler.maxServiceMs() << "\n";
        ofs << "\n";
    } else {
        std::cerr << "[WARN] Could not open metrics.log for writing.\n";
    }
}

inline void handleSum(ThreadPoolScheduler& scheduler, long long N) {
    // Submit a sum task; it will print its own result
    scheduler.submit([N]() -> void {
        long long sum = 0;
        for (long long i = 1; i <= N; ++i) {
            sum += i;
        }
        std::cout << "[Task sum " << N << "] result = " << sum
                  << " (thread " << std::this_thread::get_id() << ")\n";
    });

    std::cout << "Scheduled sum task for N = " << N << "\n";
}

inline void handleSleep(ThreadPoolScheduler& scheduler, int ms) {
    scheduler.submit([ms]() -> void {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        std::cout << "[Task sleep " << ms << "ms] done"
                  << " (thread " << std::this_thread::get_id() << ")\n";
    });

    std::cout << "Scheduled sleep task for " << ms << " ms\n";
}

inline void handleBenchmark(ThreadPoolScheduler& scheduler) {
    using namespace std::chrono;

    const int nTasks = 5000;  // per workload type
    std::cout << "Running benchmarks with " << nTasks
              << " tasks per workload type (CPU, memory, mixed)...\n";

    auto cpuHeavy = [](int iters) {
        double acc = 0.0;
        for (int i = 0; i < iters; ++i) {
            acc += std::sin(i * 0.001) * std::cos(i * 0.002);
        }
        volatile double sink = acc;
        (void)sink;
    };

    auto memoryHeavy = [](int size) {
        std::vector<int> v(size);
        std::iota(v.begin(), v.end(), 0);
        std::mt19937 rng{std::random_device{}()};
        std::shuffle(v.begin(), v.end(), rng);
        std::sort(v.begin(), v.end());
        volatile int sink = v[0];
        (void)sink;
    };

    auto mixedHeavy = [&](int iters, int size) {
        cpuHeavy(iters / 2);
        memoryHeavy(size / 10);
    };

    auto timeSingle = [&](auto&& fn) -> long long {
        auto start = steady_clock::now();
        for (int i = 0; i < nTasks; ++i) {
            fn();
        }
        auto end = steady_clock::now();
        return duration_cast<milliseconds>(end - start).count();
    };

    auto timePool = [&](auto&& fn) -> long long {
        auto start = steady_clock::now();
        std::vector<std::future<void>> futures;
        futures.reserve(nTasks);
        for (int i = 0; i < nTasks; ++i) {
            futures.push_back(
                scheduler.submit([&fn]() -> void {
                    fn();
                })
            );
        }
        for (auto& f : futures) {
            f.get();
        }
        auto end = steady_clock::now();
        return duration_cast<milliseconds>(end - start).count();
    };

    struct ResultRow {
        std::string name;
        long long baselineMs;
        long long poolMs;
    };

    std::vector<ResultRow> results;

    {
        int iters = 50000;
        auto baselineMs = timeSingle([&]() { cpuHeavy(iters); });
        auto poolMs     = timePool([&]() { cpuHeavy(iters); });
        results.push_back({"cpu_heavy", baselineMs, poolMs});
    }

    {
        int size = 20000;
        auto baselineMs = timeSingle([&]() { memoryHeavy(size); });
        auto poolMs     = timePool([&]() { memoryHeavy(size); });
        results.push_back({"memory_heavy", baselineMs, poolMs});
    }

    {
        int iters = 50000;
        int size  = 20000;
        auto baselineMs = timeSingle([&]() { mixedHeavy(iters, size); });
        auto poolMs     = timePool([&]() { mixedHeavy(iters, size); });
        results.push_back({"mixed", baselineMs, poolMs});
    }

    std::cout << "=== Benchmark Results ===\n";
    for (const auto& row : results) {
        std::cout << "Workload: " << row.name << "\n";
        std::cout << "  Baseline (single-thread): " << row.baselineMs << " ms\n";
        std::cout << "  ThreadPool:               " << row.poolMs     << " ms\n";
        if (row.poolMs > 0) {
            double speedup = static_cast<double>(row.baselineMs) / row.poolMs;
            std::cout << "  Speedup: " << speedup << "x\n";
        } else {
            std::cout << "  Speedup: N/A (poolMs == 0)\n";
        }
        std::cout << "\n";
    }

    std::ofstream ofs("benchmark_results.csv", std::ios::app);
    if (ofs) {
        for (const auto& row : results) {
            double speedup = (row.poolMs > 0)
                             ? static_cast<double>(row.baselineMs) / row.poolMs
                             : 0.0;
            ofs << row.name << ","
                << nTasks     << ","
                << row.baselineMs << ","
                << row.poolMs     << ","
                << speedup        << "\n";
        }
    } else {
        std::cerr << "[WARN] Could not open benchmark_results.csv for writing.\n";
    }
}