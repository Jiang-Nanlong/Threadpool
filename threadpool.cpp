//
// Created by cml on 24-12-11.
//
#include "threadpool.h"

#include <iostream>
#include <utility>
#include <thread>

uint32_t Thread::threadCount_ = 0;

void Semaphore::wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [&] { return count_ > 0; });
    --count_;
}

void Semaphore::signal() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++count_;
    condition_.notify_all();
}

Result::Result(std::shared_ptr<Task> task, bool isValid): task_(std::move(task)),
                                                          isValid_(isValid) {
    task_->setResult(this);
}

Any Result::get() {
    if (!isValid_)
        return "";
    sem_.wait();
    return std::move(any_);
}

void Result::setAny(Any any) {
    this->any_ = std::move(any);
    sem_.signal();
}

Task::Task(): result_(nullptr) {
}

void Task::excute() {
    if (result_) {
        result_->setAny(std::move(run()));
    }
}

void Task::setResult(Result *res) {
    result_ = res;
}

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
    while (isRunning_) {
        // std::cout<<"thread: "<<std::this_thread::get_id()<<"已就绪"<<std::endl;
        std::shared_ptr<Task> task; {
            std::unique_lock<std::mutex> lock(mutex_);
            // 这里如果一个线程阻塞了太长时间就应该释放它
            while (tasks_.empty()) {
                if (poolMode_ == PoolMode::MODE_CACHED) {
                    // 这个地方有个问题，交换下边两个if语句以后就会把所有的线程都释放掉
                    // if (std::cv_status::timeout == notEmptyCondition_.wait_for(
                    //         lock, std::chrono::seconds(MAX_THREAD_DESTROY_INTERVAL))) {
                    //     if (currentThreadSize_ > initThreadSize_) {
                    //         std::cout << "ThreadPool::threadFunc(), currentThreadSize: " << currentThreadSize_ <<
                    //                 std::endl;
                    //         threads_.erase(threadId);
                    //         --idleThreadSize_;
                    //         --currentThreadSize_;
                    //         return;
                    //     }
                    // }
                    // 这里还是不能用上边的写法，因为在剩余最后initThreadSize_个线程时，没法阻塞住
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
                if (!isRunning_) {
                    threads_.erase(threadId);
                    // 其实这两个变量更不更新无所谓，反正都是释放所有线程
                    --idleThreadSize_;
                    --currentThreadSize_;
                    exitCondition_.notify_all();
                    std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() <<
                            "  exit" <<
                            std::endl;
                    return;
                }
            }

            --idleThreadSize_;
            std::cout << idleThreadSize_ << std::endl;
            task = tasks_.front();
            tasks_.pop();
            --taskSize_;
            // 如果队列中还有任务，通知其他线程来处理任务
            if (!tasks_.empty()) {
                notEmptyCondition_.notify_all();
            }

            // 通知可以继续提交任务
            notFullCondition_.notify_all();
        }
        if (task)
            task->excute();
        ++idleThreadSize_;
        lasttime = std::chrono::high_resolution_clock::now();
        std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() << "  complished" <<
                std::endl;
    }
    threads_.erase(threadId);
    // 其实这两个变量更不更新无所谓，反正都是释放所有线程
    --idleThreadSize_;
    --currentThreadSize_;
    exitCondition_.notify_all();
    std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() <<
            "  exit" <<
            std::endl;
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

ThreadPool::~ThreadPool() {
    isRunning_ = false;
    notEmptyCondition_.notify_all();

    std::unique_lock<std::mutex> lock(mutex_);
    exitCondition_.wait(lock, [&] { return threads_.empty(); });
}

void ThreadPool::start(int threadnum) {
    isRunning_ = true;
    initThreadSize_ = threadnum;

    for (int i = 0; i < initThreadSize_; ++i) {
        //auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        auto ptr = std::make_unique<Thread>([this](int &&PH1) { threadFunc(std::forward<decltype(PH1)>(PH1)); });
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

Result ThreadPool::submitTask(const std::shared_ptr<Task> &task) {
    // 获取锁
    // 线程通信，等待任务队列有空余位置
    // 如果有空余了，就把任务放入队列
    // 放入成功以后，在notEmpty_信号量上通知
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!notFullCondition_.wait_for(lock,
                                        std::chrono::seconds(1),
                                        [this] { return tasks_.size() < taskQueueThreshold_; })) {
            // 到时间结束也没有满足条件
            std::cerr << "ThreadPool::submitTask() failed" << std::endl;
            return Result(task, false);
        }
        tasks_.emplace(task);
        ++taskSize_;

        notEmptyCondition_.notify_all();
    }
    // 创建新线程
    if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && currentThreadSize_ <
        threadSizeThreshold_) {
        //auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        auto ptr = std::make_unique<Thread>([this](int &&PH1) { threadFunc(std::forward<decltype(PH1)>(PH1)); });
        uint32_t threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
        threads_[threadId]->start();
        ++idleThreadSize_;
        ++currentThreadSize_;
        std::cout << "new thread create" << std::endl;
        std::cout << idleThreadSize_ << ":" << currentThreadSize_ << std::endl;
    }
    return Result(task);
}
