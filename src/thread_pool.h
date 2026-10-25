// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cstddef>
#include <cstdint>

#include <mutex>
#include <thread>

#include <utility>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>

class ThreadPool {
public:
    ThreadPool(std::size_t numThreads);

    ~ThreadPool();

    /// Waits for queued work, then adds or retires workers to match `numThreads`.
    /// An unchanged count leaves existing workers untouched.
    /// @pre Calls must be serialized, with no concurrent work submissions, and
    ///      made outside pool workers. `numThreads` must be positive.
    void setThreadCount(std::size_t numThreads);

    void assignWork(std::move_only_function<void()> work);

    class TaskGroup;

    void wait();
private:
    bool _stopping = false;
    uint32_t _numActiveTasks = 0;
    std::mutex _mutex;
    std::condition_variable_any _availableWorkCV;
    std::condition_variable _waitCV;
    std::vector<std::jthread> _threads;
    std::queue<std::move_only_function<void()>> _workQueue;
};

class ThreadPool::TaskGroup {
public:
    TaskGroup() = default;
    TaskGroup(const TaskGroup&) = delete;
    TaskGroup& operator=(const TaskGroup&) = delete;
    TaskGroup(TaskGroup&&) = delete;
    TaskGroup& operator=(TaskGroup&&) = delete;

    void assignWork(ThreadPool& pool, std::move_only_function<void()> work) {
        {
            std::lock_guard lock(_mutex);
            ++_pending;
        }
        pool.assignWork([this, work = std::move(work)]() mutable {
            work();
            std::lock_guard lock(_mutex);
            --_pending;
            _waitCV.notify_all();
        });
    }

    void wait() {
        std::unique_lock lock(_mutex);
        _waitCV.wait(lock, [this] { return _pending == 0; });
    }

private:
    std::size_t _pending = 0;
    std::mutex _mutex;
    std::condition_variable _waitCV;
};