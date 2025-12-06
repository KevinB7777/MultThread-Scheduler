#include <iostream>
#include "thread_pool.h"
#include "interactive_shell.h"

int main() {
    const std::size_t numThreads = 4;

    std::cout << "Starting ThreadPoolScheduler with " << numThreads << " threads.\n";
    ThreadPoolScheduler scheduler(numThreads);

    runInteractiveShell(scheduler);

    std::cout << "Final metrics:\n";
    std::cout << "  Tasks submitted:   " << scheduler.tasksSubmitted() << "\n";
    std::cout << "  Tasks completed:   " << scheduler.tasksCompleted() << "\n";
    std::cout << "  Uptime:            " << scheduler.uptimeSeconds() << " s\n";
    std::cout << "  Throughput:        " << scheduler.throughputTasksPerSecond() << " tasks/s\n";
    std::cout << "  Avg wait time:     " << scheduler.avgWaitMs() << " ms\n";
    std::cout << "  Avg service time:  " << scheduler.avgServiceMs() << " ms\n";

    scheduler.shutdown();
    return 0;
}