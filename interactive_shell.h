#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include "thread_pool.h"
#include "cli_commands.h"

// The shell is responsible for:
// - printing the prompt
// - reading a line
// - parsing the command
// - delegating to the right handler function

inline void runInteractiveShell(ThreadPoolScheduler& scheduler) {
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
            handleMetrics(scheduler);
        } else if (cmd == "sum") {
            long long N;
            if (!(iss >> N) || N <= 0) {
                std::cout << "Usage: sum N   (N must be positive)\n";
                continue;
            }
            handleSum(scheduler, N);
        } else if (cmd == "sleep") {
            int ms;
            if (!(iss >> ms) || ms < 0) {
                std::cout << "Usage: sleep ms   (ms must be non-negative)\n";
                continue;
            }
            handleSleep(scheduler, ms);
        } else if (cmd == "benchmark") {
            handleBenchmark(scheduler);
        } else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for commands.\n";
        }
    }
}