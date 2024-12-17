//
// Created by cml on 24-12-11.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <memory>
#include <condition_variable>
#include <functional>

#define USE_TIMEOUT_STRATEGY true

constexpr int MAX_THREADS = 8;
constexpr int MAX_QUEUE_SIZE = 1024;
constexpr int MAX_THREAD_DESTROY_INTERVAL = 10; // 释放线程的时间间隔，单位秒

enum class PoolMode {
    MODE_FIXED,
    MODE_CACHED,
};

// 实现了一个信号量类，其实这里可以用C++20的信号量代替
class Semaphore {
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic_int count_;

public:
    explicit Semaphore(int count = 0) : count_(count) {
    };

    ~Semaphore() = default;

    Semaphore(const Semaphore &) = delete;

    Semaphore &operator=(const Semaphore &) = delete;

    Semaphore(Semaphore &&) = delete;

    Semaphore &operator=(Semaphore &&) = delete;

    void wait();

    void signal();
};

class Any {
public:
    Any() = default;

    Any(const Any &) = delete;

    Any &operator=(const Any &) = delete;

    Any(Any &&) = default;

    Any &operator=(Any &&) = default;

    template<typename T>
    Any(T val): base_(std::make_unique<Derived<T> >(val)) {
    }

    ~Any() = default;

    template<typename T>
    T cast_() const {
        // dynamic_cast支持RTTI
        Derived<T> *pd = dynamic_cast<Derived<T> *>(base_.get());
        if (pd == nullptr)
            throw std::bad_cast();
        return pd->value;
    }

private:
    class Base {
    public:
        virtual ~Base() = default;
    };

    template<typename T>
    class Derived : public Base {
    public:
        T value;

    public:
        Derived(T value) : value(value) {
        }
    };

private:
    std::unique_ptr<Base> base_;
};

class Task;

// Result的生命周期长于Task
class Result {
private:
    Any any_;
    // 要把Task对象和Result对象绑定起来
    // 上边是第一个原因，还有一个原因是，task的生存期要等到用户读到他的结果以后才可以释放，所以此处要用智能指针来绑定task不让它释放
    std::shared_ptr<Task> task_;
    bool isValid_; // 这个地方感觉没必要用原子类型，涉及该变量的地方只有提交和获取结果两个地方，但这两个地方应该都在同一线程中
    Semaphore sem_;

public:
    explicit Result(std::shared_ptr<Task> task, bool isValid = true);

    Result(Result &&) = default;

    ~Result() = default;

    Any get();

    void setAny(Any);
};

// 任务的抽象基类
class Task {
private:
    Result *result_;

public:
    Task();

    virtual ~Task() = default;

    virtual Any run() = 0;

    void excute();

    void setResult(Result *res);
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
    // std::vector<std::unique_ptr<Thread> > threads_;
    std::unordered_map<uint32_t, std::unique_ptr<Thread> > threads_;
    int initThreadSize_;
    std::atomic_int idleThreadSize_;
    std::atomic_int currentThreadSize_;
    int threadSizeThreshold_;

    std::queue<std::shared_ptr<Task> > tasks_;
    std::atomic_uint taskSize_;
    int taskQueueThreshold_;

    std::mutex mutex_;
    std::condition_variable notFullCondition_;
    std::condition_variable notEmptyCondition_;

    PoolMode poolMode_;

    std::atomic_bool isRunning_;

private:
    // 从任务队列中消费任务
    void threadFunc(uint32_t threadId);

public:
    ThreadPool();

    ~ThreadPool() = default;

    void start(int threadnum);

    void setMode(const PoolMode &mode);

    void setTaskQueueThreshold(int taskQueueThreshold);

    void setThreadThreshold(int threadThreshold);

    // 向任务队列中添加任务
    Result submitTask(const std::shared_ptr<Task> &task);

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = delete;
};
#endif //THREADPOOL_H
