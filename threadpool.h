//
// Created by cml on 24-12-11.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <queue>
#include <vector>
#include <memory>
#include <condition_variable>
#include <functional>

enum class PoolMode {
    MODE_FIXED,
    MODE_CACHED,
};

// 任务的抽象基类
class Task {
public:
    virtual ~Task() = default;

    virtual void run() = 0;
};

class Thread {
public:
    using func=std::function<void()>;

private:
    func func_;

public:
    explicit Thread(const func &func);

    ~Thread()=default;

    void start();

};

class ThreadPool {
private:
    std::vector<std::unique_ptr<Thread>> threads_;
    int threadSize_;

    std::queue<std::shared_ptr<Task>> tasks_;
    std::atomic_uint taskSize_;
    int taskQueueThreshold_;

    std::mutex mutex_;
    std::condition_variable notFullCondition_;
    std::condition_variable notEmptyCondition_;

    PoolMode poolMode_;

    // 从任务队列中消费任务
    void threadFunc();

public:
    ThreadPool();

    ~ThreadPool()=default;

    void start(int threadnum);

    void setMode(const PoolMode& mode);

    void setTaskQueueThreshold(int taskQueueThreshold);

    // 向任务队列中添加任务
    void submitTask(const std::shared_ptr<Task>& task);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
};
#endif //THREADPOOL_H
