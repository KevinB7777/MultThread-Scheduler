# C++17 Multithreaded Task Scheduler  
*A lightweight thread pool with futures, an interactive CLI shell, and real metrics.*

## Overview

This project implements a **modern C++17 multithreaded task scheduler** featuring:

- A fixed-size **thread pool**
- A thread-safe **task queue**
- A generic `submit()` API returning `std::future<T>`
- Condition variables & mutex synchronization
- RAII-safe thread lifecycle management
- Atomic metrics
- An **interactive terminal shell**

# HOW TO COMPILE

### **Important:**  
Because the scheduler is completely header-only,  
**you compile only `main.cpp`**, but must link pthreads.

Run this exact command:

```bash
g++ -std=c++17 main.cpp -pthread -O2 -o scheduler_shell
```clang
clang++ -std=c++17 main.cpp -pthread -O2 -o scheduler_shell

Then run:
./scheduler_shell