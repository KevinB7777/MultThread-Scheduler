#include <iostream>
#include "thread_pool.h"
#include "interactive_cli.h"

int main() {
    const std::size_t numThreads = 4;

    std::cout << "Starting ThreadPoolScheduler with " << numThreads << " threads.\n";
    ThreadPoolScheduler scheduler(numThreads);

    runInteractiveShell(scheduler);

    std::cout << "Final metrics:\n";
    std::cout << "  Tasks submitted:  " << scheduler.tasksSubmitted() << "\n";
    std::cout << "  Tasks completed: " << scheduler.tasksCompleted() << "\n";

    scheduler.shutdown();
    return 0;
}