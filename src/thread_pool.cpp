// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "thread_pool.h"

#include <format>

#include "profiling.h"
#include "system.h"

ThreadPool::ThreadPool(std::size_t numThreads) {
    setThreadCount(numThreads);
}

void ThreadPool::setThreadCount(std::size_t numThreads) {
    if (numThreads == _threads.size())
        return;
    wait();
    if (numThreads < _threads.size()) {
        for (std::size_t i = numThreads; i < _threads.size(); ++i)
            _threads[i].request_stop();
        _threads.resize(numThreads);
        return;
    }

    _threads.reserve(numThreads);
    for (std::size_t i = _threads.size(); i < numThreads; ++i) {
        _threads.emplace_back([this, i](std::stop_token stopToken) {
            javelin::system::setThreadName(
                std::format("ThreadPool #{}", i).c_str());
            while(true) {
                std::move_only_function<void()> workUnit;
                {
                    JAVELIN_ZONE_SCOPED_N("Waiting for queued work");
                    std::unique_lock lock(_mutex);
                    const bool ready = _availableWorkCV.wait(
                        lock,
                        stopToken,
                        [&] { return _stopping || !_workQueue.empty(); });

                    if (!ready || (_stopping && _workQueue.empty()))
                        break;

                    workUnit = std::move(_workQueue.front());
                    _workQueue.pop();
                }
                {
                    JAVELIN_ZONE_SCOPED_N("Running work unit");
                    workUnit();
                }
                {
                    JAVELIN_ZONE_SCOPED_N("Notify work completed");
                    std::lock_guard lock(_mutex);
                    --_numActiveTasks;
                    if(_numActiveTasks == 0 && _workQueue.empty())
                        _waitCV.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
    std::lock_guard lock(_mutex);
    _stopping = true;
    }
    _availableWorkCV.notify_all();
    for (std::jthread& thread : _threads) {
        thread.join();
    }
}

void ThreadPool::assignWork(std::move_only_function<void()> work) {
    {
        std::lock_guard lock(_mutex);
        _workQueue.push(std::move(work));
        ++_numActiveTasks;
    }
    _availableWorkCV.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock lock(_mutex);
    _waitCV.wait(lock, [this] {
        return _numActiveTasks == 0 && _workQueue.empty();
    });
}
