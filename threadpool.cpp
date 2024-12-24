//
// Created by cml on 24-12-11.
//
#include "threadpool.h"


uint32_t Thread::threadCount_ = 0;

Thread::Thread(const func &func): threadId_(++threadCount_) {
    func_ = func;
}

void Thread::start() {
    std::thread t(func_, threadId_);
    t.detach();
}

uint32_t Thread::getThreadId() const {
    return threadId_;
}

void ThreadPool::threadFunc(uint32_t threadId) {
    std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() << "  is ready" <<
            std::endl;
    auto lasttime = std::chrono::high_resolution_clock::now();
    while (true) {
        Task task;
        //
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (taskSize_.load() == 0) {
                if (!isRunning_) {
                    threads_.erase(threadId);
                    --idleThreadSize_;
                    --currentThreadSize_;
                    std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() <<
                            "  exit" <<
                            std::endl;
                    exitCondition_.notify_all();
                    return;
                }
                if (poolMode_ == PoolMode::MODE_CACHED) {
                    if (std::cv_status::timeout == notEmptyCondition_.wait_for(lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto time = std::chrono::duration_cast<std::chrono::seconds>(now - lasttime);
                        if (time.count() > MAX_THREAD_DESTROY_INTERVAL && currentThreadSize_ > initThreadSize_) {
                            std::cout << "ThreadPool::threadFunc(), currentThreadSize: " << currentThreadSize_ <<
                                    std::endl;
                            threads_.erase(threadId);
                            --idleThreadSize_;
                            --currentThreadSize_;
                            return;
                        }
                    }
                } else {
                    // MODE_FIXED模式
                    notEmptyCondition_.wait(lock);
                }
            }
            task = tasks_.front();
            tasks_.pop();

            --taskSize_;
            // 如果队列中还有任务，通知其他线程来处理任务
            if (taskSize_.load() > 0) {
                notEmptyCondition_.notify_all();
            }
            // 通知可以继续提交任务
            notFullCondition_.notify_all();
        }

        --idleThreadSize_;
        // std::cout << "idleThreadSize_: " << idleThreadSize_ << std::endl;
        if (task)
            task();
        ++idleThreadSize_;
        lasttime = std::chrono::high_resolution_clock::now();
        std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() << "  complished" <<
                std::endl;
    }
}

ThreadPool::ThreadPool(): initThreadSize_(0),
                          idleThreadSize_(0),
                          currentThreadSize_(0),
                          threadSizeThreshold_(MAX_THREADS),
                          taskSize_(0),
                          taskQueueThreshold_(MAX_QUEUE_SIZE),
                          poolMode_(PoolMode::MODE_FIXED),
                          isRunning_(false) {
}

ThreadPool &ThreadPool::getInstance() {
    static ThreadPool pool;
    return pool;
}

ThreadPool::~ThreadPool() {
    isRunning_ = false;

    std::unique_lock<std::mutex> lock(mutex_);
    // notEmptyCondition_.notify_all尽可能的往后靠，在线程的退出时有用
    notEmptyCondition_.notify_all();
    exitCondition_.wait(lock, [&] { return threads_.empty(); });
}

void ThreadPool::start(uint8_t threadnum) {
    if (isRunning_)
        return;

    isRunning_ = true;
    initThreadSize_ = std::min(static_cast<int>(threadnum), threadSizeThreshold_);

    for (int i = 0; i < initThreadSize_; ++i) {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        // auto ptr = std::make_unique<Thread>([this](int &&PH1) { threadFunc(std::forward<decltype(PH1)>(PH1)); });
        uint32_t threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
    }

    for (auto &thread: threads_) {
        thread.second->start();
        ++currentThreadSize_;
        ++idleThreadSize_;
    }
}

void ThreadPool::setMode(const PoolMode &mode) {
    if (isRunning_)
        return;
    poolMode_ = mode;
}

void ThreadPool::setTaskQueueThreshold(int taskQueueThreshold) {
    if (isRunning_)
        return;
    taskQueueThreshold_ = taskQueueThreshold;
}

void ThreadPool::setThreadThreshold(int threadThreshold) {
    if (isRunning_)
        return;
    threadSizeThreshold_ = threadThreshold;
}

