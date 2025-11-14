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

class ThreadPoolScheduler {
public:
    explicit ThreadPoolScheduler(std::size_t numThreads)
        : stop_(false),
          tasksSubmitted_(0),
          tasksCompleted_(0)
    {
        if (numThreads == 0) {
            throw std::invalid_argument("numThreads must be > 0");
        }

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

            tasks_.emplace([taskPtr]() {
                (*taskPtr)();  // run the task and set the promise result
            });
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

    // Simple metrics API
    std::uint64_t tasksSubmitted() const noexcept {
        return tasksSubmitted_.load();
    }

    std::uint64_t tasksCompleted() const noexcept {
        return tasksCompleted_.load();
    }

private:
    void workerLoop(std::size_t workerId) {
        (void)workerId; // unused for now, but kept for future logging

        while (true) {
            std::function<void()> task;

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

            // Run task outside the lock
            task();
            ++tasksCompleted_;
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;

    std::atomic<std::uint64_t> tasksSubmitted_;
    std::atomic<std::uint64_t> tasksCompleted_;
};