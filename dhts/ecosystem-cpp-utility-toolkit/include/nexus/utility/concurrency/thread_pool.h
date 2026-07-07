#pragma once

#include <vector>
#include <deque>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <type_traits>
#include <stdexcept>
#include <random>
#include <list>

namespace nexus::utility::concurrency {

/// @brief Work-stealing thread pool with priority support.
///
/// Thread safety: fully thread-safe. All public methods can be called from any thread.
///
/// The pool uses a shared global queue for high-priority tasks and per-worker
/// deques for work-stealing of normal/low-priority tasks. Workers steal from
/// other workers' deques when their own is empty, providing load balancing.
class ThreadPool {
public:
    enum class Priority { Low = 0, Normal = 1, High = 2, Critical = 3 };

    /// @brief Create a thread pool with the specified number of workers.
    /// @param num_threads Number of worker threads (0 = hardware concurrency)
    explicit ThreadPool(size_t num_threads = 0)
        : running_(true), active_tasks_(0) {
        if (num_threads == 0) {
            num_threads = std::max(1u, std::thread::hardware_concurrency());
        }
        workers_.reserve(num_threads);
        // Initialize per-worker structures
        for (size_t i = 0; i < num_threads; ++i) {
            queues_.emplace_back();
            mutexes_.emplace_back();
            cvs_.emplace_back();
        }

        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back(&ThreadPool::workerLoop, this, i);
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ── Task Submission ─────────────────────────────────────────────────────

    /// @brief Submit a task and get a future for the result.
    template <typename Func, typename... Args>
    [[nodiscard]] auto submit(Priority priority, Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<ReturnType> future = task->get_future();

        if (priority == Priority::High || priority == Priority::Critical) {
            // High priority goes to global queue
            std::lock_guard lock(global_mutex_);
            global_queue_.push({priority, [task]() { (*task)(); }});
            global_cv_.notify_one();
        } else {
            // Normal/low priority goes to a random worker's deque
            size_t idx = randomWorker();
            std::lock_guard lock(mutexes_[idx]);
            queues_[idx].push_back({priority, [task]() { (*task)(); }});
            cvs_[idx].notify_one();
        }

        ++active_tasks_;
        return future;
    }

    /// @brief Submit a task with normal priority.
    template <typename Func, typename... Args>
    [[nodiscard]] auto submit(Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        return submit(Priority::Normal, std::forward<Func>(func),
                      std::forward<Args>(args)...);
    }

    /// @brief Submit a fire-and-forget task (no future).
    void execute(Priority priority, std::function<void()> func) {
        if (priority == Priority::High || priority == Priority::Critical) {
            std::lock_guard lock(global_mutex_);
            global_queue_.push({priority, std::move(func)});
            global_cv_.notify_one();
        } else {
            size_t idx = randomWorker();
            std::lock_guard lock(mutexes_[idx]);
            queues_[idx].push_back({priority, std::move(func)});
            cvs_[idx].notify_one();
        }
        ++active_tasks_;
    }

    // ── Control ─────────────────────────────────────────────────────────────

    /// @brief Wait for all submitted tasks to complete.
    void waitAll() {
        while (active_tasks_.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }
    }

    /// @brief Wait for all submitted tasks and then stop.
    void shutdown() {
        if (!running_.load(std::memory_order_acquire)) return;

        waitAll();
        running_.store(false, std::memory_order_release);

        for (auto& cv : cvs_) cv.notify_all();
        global_cv_.notify_all();

        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    /// @brief Get the number of queued tasks across all queues.
    [[nodiscard]] size_t queuedTasks() noexcept {
        size_t count = 0;
        {
            std::lock_guard lock(global_mutex_);
            count += global_queue_.size();
        }
        for (size_t i = 0; i < mutexes_.size(); ++i) {
            std::lock_guard lock(mutexes_[i]);
            count += queues_[i].size();
        }
        return count;
    }

    /// @brief Get the number of worker threads.
    [[nodiscard]] size_t workerCount() const noexcept { return workers_.size(); }

    /// @brief Get the number of currently active (running) tasks.
    [[nodiscard]] size_t activeTasks() const noexcept {
        return active_tasks_.load(std::memory_order_acquire);
    }

    /// @brief Check if the pool is running.
    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

private:
    struct QueuedTask {
        Priority priority;
        std::function<void()> func;

        bool operator<(const QueuedTask& other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };

    std::vector<std::thread> workers_;
    std::vector<std::deque<QueuedTask>> queues_;
    std::deque<std::mutex> mutexes_;
    std::deque<std::condition_variable> cvs_;

    // Global queue for high-priority tasks
    std::priority_queue<QueuedTask> global_queue_;
    mutable std::mutex global_mutex_;
    std::condition_variable global_cv_;

    std::atomic<bool> running_;
    std::atomic<size_t> active_tasks_;
    std::mt19937_64 rng_{std::random_device{}()};

    [[nodiscard]] size_t randomWorker() noexcept {
        std::uniform_int_distribution<size_t> dist(0, mutexes_.size() - 1);
        return dist(rng_);
    }

    void workerLoop(size_t worker_id) {
        while (running_.load(std::memory_order_acquire)) {
            QueuedTask task;

            // 1. Try own deque first
            if (tryPopOwn(worker_id, task)) {
                executeTask(task);
                continue;
            }

            // 2. Try global high-priority queue
            if (tryPopGlobal(task)) {
                executeTask(task);
                continue;
            }

            // 3. Try stealing from another worker
            if (trySteal(worker_id, task)) {
                executeTask(task);
                continue;
            }

            // 4. Nothing to do - wait
            {
                std::unique_lock lock(mutexes_[worker_id]);
                cvs_[worker_id].wait_for(lock, std::chrono::milliseconds(1));
            }
        }
    }

    bool tryPopOwn(size_t id, QueuedTask& task) {
        std::lock_guard lock(mutexes_[id]);
        if (queues_[id].empty()) return false;

        // Find highest priority task
        auto best = queues_[id].begin();
        for (auto it = queues_[id].begin(); it != queues_[id].end(); ++it) {
            if (it->priority > best->priority) best = it;
        }
        task = std::move(*best);
        queues_[id].erase(best);
        return true;
    }

    bool tryPopGlobal(QueuedTask& task) {
        std::lock_guard lock(global_mutex_);
        if (global_queue_.empty()) return false;
        task = std::move(const_cast<QueuedTask&>(global_queue_.top()));
        global_queue_.pop();
        return true;
    }

    bool trySteal(size_t thief_id, QueuedTask& task) {
        for (size_t i = 0; i < mutexes_.size(); ++i) {
            if (i == thief_id) continue;
            std::lock_guard lock(mutexes_[i]);
            if (!queues_[i].empty()) {
                task = std::move(queues_[i].back());
                queues_[i].pop_back();
                return true;
            }
        }
        return false;
    }

    void executeTask(QueuedTask& task) {
        task.func();
        active_tasks_.fetch_sub(1, std::memory_order_release);
    }
};

} // namespace nexus::utility::concurrency
