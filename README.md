# 🚀 C++17 Multithreaded Task Scheduler

*A high-performance thread-pool scheduler with an interactive shell, real metrics, and Google Benchmark validation.*

---

## Features

✔ Modern **C++17 thread pool** with safe shutdown  
✔ **Task submission with return values** via `std::future<T>`  
✔ Efficient scheduling with **condition variables + mutexes**  
✔ Concurrency-safe counters using **atomics**  
✔ Built-in **performance metrics**:
- Total submitted / completed
- Avg & max wait time
- Avg & max run time  
✔ **Interactive CLI**: submit jobs *live* from shell  
✔ **Benchmark suite** for scientific performance testing  
✔ Designed using **RAII principles** for thread lifetime safety

---

## CLI Commands

Inside the running shell:

| Command | Description |
|--------|-------------|
| `sum N` | Compute sum of 1..N asynchronously |
| `sleep ms` | Sleep task (simulates blocking) |
| `metrics` | Display real-time scheduler performance counters |
| `benchmark` | Run built-in workload benchmark (CPU, memory, mixed) |
| `help` | Show available commands |
| `exit` / `quit` | Shutdown scheduler |
