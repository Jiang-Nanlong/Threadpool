//
// Created by cml on 24-12-11.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <memory>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <utility>
#include <thread>

constexpr int MAX_THREADS = 200;
constexpr int MAX_QUEUE_SIZE = 2;
constexpr int MAX_THREAD_DESTROY_INTERVAL = 10; // 释放线程的时间间隔，单位秒

enum class PoolMode {
    MODE_FIXED,
    MODE_CACHED,
};


class Thread {
public:
    using func = std::function<void(uint32_t)>;

private:
    func func_;

    uint32_t threadId_;

    static uint32_t threadCount_;

public:
    explicit Thread(const func &func);

    ~Thread() = default;

    void start();

    uint32_t getThreadId() const;
};

class ThreadPool {
private:
    std::unordered_map<uint32_t, std::unique_ptr<Thread> > threads_;
    int initThreadSize_;
    std::atomic_int idleThreadSize_;
    std::atomic_int currentThreadSize_;
    int threadSizeThreshold_;

    using Task = std::function<void()>;

    std::queue<Task> tasks_;
    std::atomic_uint taskSize_;
    int taskQueueThreshold_;

    std::mutex mutex_;
    std::condition_variable notFullCondition_;
    std::condition_variable notEmptyCondition_;

    PoolMode poolMode_;

    std::atomic_bool isRunning_;
    std::condition_variable exitCondition_;

private:
    void threadFunc(uint32_t threadId);

    ThreadPool();

public:
    static ThreadPool &getInstance();

    ~ThreadPool();

    void start(uint8_t threadnum = std::thread::hardware_concurrency());

    void setMode(const PoolMode &mode = PoolMode::MODE_FIXED);

    void setTaskQueueThreshold(int taskQueueThreshold);

    void setThreadThreshold(int threadThreshold);

    template<typename F, typename... Args>
    auto submitTask(F &&f, Args &&... args) -> std::future<decltype(f(args...))>;

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(ThreadPool &&) = delete;
};

template<typename F, typename... Args>
auto ThreadPool::submitTask(F &&f, Args &&... args) -> std::future<decltype(f(args...))> {
    using RetType = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<RetType()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<RetType> res = task->get_future();
    //

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!notFullCondition_.wait_for(lock, std::chrono::seconds(1),
                                        [&] { return taskSize_.load() < taskQueueThreshold_; })) {
            // 到时间结束也没有满足条件
            std::cerr << "ThreadPool::submitTask() failed" << std::endl;
            std::packaged_task<RetType()> emptytask([] { return RetType(); });
            auto ret = emptytask.get_future();
            emptytask();
            return ret;
        }
        tasks_.emplace([=] { (*task)(); });
        ++taskSize_;

        notEmptyCondition_.notify_all();
    }
    // 创建新线程
    if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && currentThreadSize_ <
        threadSizeThreshold_) {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        // auto ptr = std::make_unique<Thread>([this](int &&PH1) { threadFunc(std::forward<decltype(PH1)>(PH1)); });
        uint32_t threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
        threads_[threadId]->start();
        ++idleThreadSize_;
        ++currentThreadSize_;
        std::cout << "new thread create" << std::endl;
        std::cout << idleThreadSize_ << ":" << currentThreadSize_ << std::endl;
    }
    return res;
}
#endif //THREADPOOL_H
