#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <ctime>
#include <sstream>
#include <map>
#include <thread>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <iostream>

namespace nexus::utility::scheduling {

/// Cron-style scheduler for recurring tasks
class CronScheduler {
public:
    struct CronExpression {
        std::string minute = "*";       // 0-59
        std::string hour = "*";         // 0-23
        std::string day_of_month = "*"; // 1-31
        std::string month = "*";        // 1-12
        std::string day_of_week = "*";  // 0-6 (Sun=0)

        [[nodiscard]] std::string toString() const {
            return minute + " " + hour + " " + day_of_month + " " + month + " " + day_of_week;
        }

        [[nodiscard]] static CronExpression parse(const std::string& expr) {
            CronExpression cron;
            std::istringstream iss(expr);
            iss >> cron.minute >> cron.hour >> cron.day_of_month >> cron.month >> cron.day_of_week;
            return cron;
        }
    };

    struct ScheduledTask {
        int id;
        CronExpression cron;
        std::function<void()> callback;
        std::string name;
    };

    CronScheduler() = default;

    ~CronScheduler() {
        stop();
    }

    CronScheduler(const CronScheduler&) = delete;
    CronScheduler& operator=(const CronScheduler&) = delete;

    /// Start the scheduler worker thread
    void start() {
        if (running_.load(std::memory_order_acquire)) {
            return;
        }
        running_.store(true, std::memory_order_release);
        worker_ = std::thread(&CronScheduler::run, this);
    }

    /// Stop the scheduler
    void stop() {
        running_.store(false, std::memory_order_release);
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    /// Add a cron-scheduled task
    int add(const std::string& cron_expr, std::function<void()> callback,
            const std::string& name = "") {
        std::lock_guard lock(mutex_);
        int id = next_id_++;
        tasks_.push_back({id, CronExpression::parse(cron_expr), std::move(callback), name});
        return id;
    }

    /// Add interval-based task (every N seconds)
    int addInterval(std::chrono::seconds interval, std::function<void()> callback,
                    const std::string& name = "") {
        CronExpression expr;
        auto secs = interval.count();

        if (secs < 60) {
            // Sub-minute interval: use second-based scheduling
            int id;
            {
                std::lock_guard lock(mutex_);
                id = next_id_++;
                scheduled_intervals_[id] = {secs, std::chrono::steady_clock::now(),
                                             std::move(callback), name};
            }
            return id;
        } else if (secs < 3600) {
            expr.minute = "*/" + std::to_string(secs / 60);
        } else if (secs < 86400) {
            expr.hour = "*/" + std::to_string(secs / 3600);
        } else {
            expr.day_of_month = "*/" + std::to_string(secs / 86400);
        }
        return add(expr.toString(), std::move(callback), name);
    }

    /// Remove a task by ID
    bool remove(int id) {
        std::lock_guard lock(mutex_);
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
                                [id](const ScheduledTask& t) { return t.id == id; });
        if (it != tasks_.end()) {
            tasks_.erase(it);
            return true;
        }
        auto int_it = scheduled_intervals_.find(id);
        if (int_it != scheduled_intervals_.end()) {
            scheduled_intervals_.erase(int_it);
            return true;
        }
        return false;
    }

    /// List all tasks
    [[nodiscard]] std::vector<ScheduledTask> listTasks() const {
        std::lock_guard lock(mutex_);
        return tasks_;
    }

    [[nodiscard]] size_t taskCount() const noexcept {
        std::lock_guard lock(mutex_);
        return tasks_.size() + scheduled_intervals_.size();
    }

private:
    struct IntervalTask {
        int64_t interval_secs;
        std::chrono::steady_clock::time_point last_fire;
        std::function<void()> callback;
        std::string name;
    };

    std::vector<ScheduledTask> tasks_;
    std::map<int, IntervalTask> scheduled_intervals_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    int next_id_ = 1;

    void run() {
        while (running_.load(std::memory_order_acquire)) {
            auto now = std::chrono::system_clock::now();
            auto now_c = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
            localtime_r(&now_c, &tm_buf);

            // Check cron tasks
            std::vector<ScheduledTask> due;
            {
                std::lock_guard lock(mutex_);
                for (const auto& task : tasks_) {
                    if (matches(task.cron, &tm_buf)) {
                        due.push_back(task);
                    }
                }
            }

            for (const auto& task : due) {
                try {
                    task.callback();
                } catch (const std::exception& e) {
                    std::cerr << "[CronScheduler] Task #" << task.id
                              << " failed: " << e.what() << "\n";
                } catch (...) {
                    std::cerr << "[CronScheduler] Task #" << task.id
                              << " failed with unknown exception\n";
                }
            }

            // Check interval tasks
            {
                auto steady_now = std::chrono::steady_clock::now();
                std::lock_guard lock(mutex_);
                for (auto& [id, task] : scheduled_intervals_) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        steady_now - task.last_fire).count();
                    if (elapsed >= task.interval_secs) {
                        task.last_fire = steady_now;
                        try {
                            task.callback();
                        } catch (const std::exception& e) {
                            std::cerr << "[CronScheduler] Interval task #" << id
                                      << " failed: " << e.what() << "\n";
                        } catch (...) {
                            std::cerr << "[CronScheduler] Interval task #" << id
                                      << " failed with unknown exception\n";
                        }
                    }
                }
            }

            // Sleep until next second
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, std::chrono::seconds(1));
        }
    }

    [[nodiscard]] static bool matches(const CronExpression& expr, const std::tm* tm) noexcept {
        return matchesField(expr.minute, tm->tm_min) &&
               matchesField(expr.hour, tm->tm_hour) &&
               matchesField(expr.day_of_month, tm->tm_mday) &&
               matchesField(expr.month, tm->tm_mon + 1) &&
               matchesField(expr.day_of_week, tm->tm_wday);
    }

    [[nodiscard]] static bool matchesField(const std::string& field, int value) noexcept {
        if (field == "*") return true;

        try {
            if (field.starts_with("*/")) {
                int interval = std::stoi(field.substr(2));
                return interval > 0 && value % interval == 0;
            }
            return std::stoi(field) == value;
        } catch (const std::exception&) {
            return false;
        }
    }
};

} // namespace nexus::utility::scheduling
