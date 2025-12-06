#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>
#include <chrono>    // for timing
#include <utility>   // for std::move

class ThreadPoolScheduler {
public:
    explicit ThreadPoolScheduler(std::size_t numThreads)
        : stop_(false),
          tasksSubmitted_(0),
          tasksCompleted_(0),
          totalWaitTimeNs_(0),
          totalServiceTimeNs_(0),
          maxWaitTimeNs_(0),
          maxServiceTimeNs_(0)
    {
        if (numThreads == 0) {
            throw std::invalid_argument("numThreads must be > 0");
        }

        startTime_ = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this, i] {
                workerLoop(i);
            });
        }
    }

    // Non-copyable (owning threads)
    ThreadPoolScheduler(const ThreadPoolScheduler&) = delete;
    ThreadPoolScheduler& operator=(const ThreadPoolScheduler&) = delete;

    // Submit a task that returns a value T (or void)
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;

        // Wrap callable into packaged_task<R()>
        auto taskPtr = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<R> fut = taskPtr->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot submit on stopped ThreadPoolScheduler");
            }

            Task t;
            t.func = [taskPtr]() {
                (*taskPtr)();  // run the task and set the promise result
            };
            t.enqueueTime = std::chrono::steady_clock::now();

            tasks_.emplace(std::move(t));   // NOTE: std::move here
            ++tasksSubmitted_;
        }

        cv_.notify_one();
        return fut;
    }

    // Graceful shutdown
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) return;  // already stopped
            stop_ = true;
        }
        cv_.notify_all();

        for (auto &t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
        workers_.clear();
    }

    ~ThreadPoolScheduler() {
        shutdown();
    }

    // Basic counters
    std::uint64_t tasksSubmitted() const noexcept {
        return tasksSubmitted_.load();
    }

    std::uint64_t tasksCompleted() const noexcept {
        return tasksCompleted_.load();
    }

    // Uptime in seconds
    double uptimeSeconds() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime_);
        return diff.count();
    }

    // Throughput = completed tasks / uptime
    double throughputTasksPerSecond() const noexcept {
        double up = uptimeSeconds();
        if (up <= 0.0) return 0.0;
        return static_cast<double>(tasksCompleted_.load()) / up;
    }

    // Average wait time in ms (enqueue -> start)
    double avgWaitMs() const noexcept {
        auto completed = tasksCompleted_.load();
        if (completed == 0) return 0.0;
        double ns = static_cast<double>(totalWaitTimeNs_.load());
        return ns / 1e6 / static_cast<double>(completed);
    }

    // Average service time in ms (start -> end)
    double avgServiceMs() const noexcept {
        auto completed = tasksCompleted_.load();
        if (completed == 0) return 0.0;
        double ns = static_cast<double>(totalServiceTimeNs_.load());
        return ns / 1e6 / static_cast<double>(completed);
    }

    // Total wait time in ms
    double totalWaitMs() const noexcept {
        double ns = static_cast<double>(totalWaitTimeNs_.load());
        return ns / 1e6;
    }

    // Total service time in ms
    double totalServiceMs() const noexcept {
        double ns = static_cast<double>(totalServiceTimeNs_.load());
        return ns / 1e6;
    }

    // Max wait time in ms
    double maxWaitMs() const noexcept {
        double ns = static_cast<double>(maxWaitTimeNs_.load());
        return ns / 1e6;
    }

    // Max service time in ms
    double maxServiceMs() const noexcept {
        double ns = static_cast<double>(maxServiceTimeNs_.load());
        return ns / 1e6;
    }

private:
    // One queued task + its enqueue timestamp
    struct Task {
        std::function<void()> func;
        std::chrono::steady_clock::time_point enqueueTime;
    };

    void workerLoop(std::size_t workerId) {
        (void)workerId; // unused for now, but kept for future logging

        while (true) {
            Task task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    // No more work and shutting down
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // Measure wait time: from enqueue to now
            auto start = std::chrono::steady_clock::now();
            auto waitNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    start - task.enqueueTime).count();
            totalWaitTimeNs_.fetch_add(waitNs, std::memory_order_relaxed);
            updateMaxAtomic(maxWaitTimeNs_, static_cast<std::uint64_t>(waitNs));

            // Run task
            task.func();

            auto end = std::chrono::steady_clock::now();
            auto serviceNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - start).count();
            totalServiceTimeNs_.fetch_add(serviceNs, std::memory_order_relaxed);
            updateMaxAtomic(maxServiceTimeNs_, static_cast<std::uint64_t>(serviceNs));

            ++tasksCompleted_;
        }
    }

    // Helper to update a max value stored in an atomic
    static void updateMaxAtomic(std::atomic<std::uint64_t>& target, std::uint64_t value) {
        auto current = target.load(std::memory_order_relaxed);
        while (value > current &&
               !target.compare_exchange_weak(current, value,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
            // current is updated by compare_exchange_weak when it fails
        }
    }

    // === MEMBER VARIABLES (must match constructor) ===

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;

    std::chrono::steady_clock::time_point startTime_;

    std::atomic<std::uint64_t> tasksSubmitted_;
    std::atomic<std::uint64_t> tasksCompleted_;

    std::atomic<std::uint64_t> totalWaitTimeNs_;
    std::atomic<std::uint64_t> totalServiceTimeNs_;
    std::atomic<std::uint64_t> maxWaitTimeNs_;
    std::atomic<std::uint64_t> maxServiceTimeNs_;
};