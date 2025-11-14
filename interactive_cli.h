#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <future>
#include "thread_pool.h"

inline void printHelp() {
    std::cout << "Commands:\n"
              << "  sum N        - schedule a task that computes sum 1..N\n"
              << "  sleep ms     - schedule a task that sleeps for ms milliseconds\n"
              << "  metrics      - show tasks submitted/completed\n"
              << "  help         - show this help message\n"
              << "  exit/quit    - exit the scheduler shell\n";
}

inline void runInteractiveShell(ThreadPoolScheduler& scheduler) {
    using namespace std::chrono_literals;

    std::cout << "=== Simple Task Scheduler Shell ===\n";
    std::cout << "Type 'help' for available commands.\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            // EOF or input error
            std::cout << "\nExiting shell (input closed).\n";
            break;
        }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty()) {
            continue;
        }

        if (cmd == "help") {
            printHelp();
        } else if (cmd == "exit" || cmd == "quit") {
            std::cout << "Shutting down scheduler and exiting...\n";
            break;
        } else if (cmd == "metrics") {
            std::cout << "Tasks submitted:  " << scheduler.tasksSubmitted() << "\n";
            std::cout << "Tasks completed: " << scheduler.tasksCompleted() << "\n";
        } else if (cmd == "sum") {
            long long N;
            if (!(iss >> N) || N <= 0) {
                std::cout << "Usage: sum N   (N must be positive)\n";
                continue;
            }

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
        } else if (cmd == "sleep") {
            int ms;
            if (!(iss >> ms) || ms < 0) {
                std::cout << "Usage: sleep ms   (ms must be non-negative)\n";
                continue;
            }

            scheduler.submit([ms]() -> void {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                std::cout << "[Task sleep " << ms << "ms] done"
                          << " (thread " << std::this_thread::get_id() << ")\n";
            });

            std::cout << "Scheduled sleep task for " << ms << " ms\n";
        } else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for commands.\n";
        }
    }
}