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
    while (true) {
        // 但是这里又会遇到一个问题，当某个线程当前正好在执行任务时，调用了线程池的析构函数，把isRunning_置为false，那么就不会进入次循环了，而是直接退出，即便是任务队列中还有任务，每个线程都是如此，那么最后任务队列不会被清空
        // 所以就不能使用isRunning_来作为循环的判断条件了，只有当任务队列中没有任务以后，才应该检查isRunning_的值
        // std::cout<<"thread: "<<std::this_thread::get_id()<<"已就绪"<<std::endl;
        std::shared_ptr<Task> task;
        //
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 这里如果一个线程阻塞了太长时间就应该释放它
            while (taskSize_.load() == 0) {
                if (!isRunning_) {
                    threads_.erase(threadId);
                    // 其实这两个变量更不更新无所谓，反正都是释放所有线程
                    --idleThreadSize_;
                    --currentThreadSize_;
                    std::cout << "ThreadPool::threadFunc(), this thread id: " << std::this_thread::get_id() <<
                            "  exit" <<
                            std::endl;
                    exitCondition_.notify_all();
                    return;
                }
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

                    // 这里是对的，当往任务队列中添加任务时，会调用notEmptyCondition_.notify_all函数唤醒所有的notEmptyCondition_.wait_for，但只有一个线程会抢到锁，然后取出任务，然后退出{}的作用域，释放锁，执行任务。
                    // 此时，其余被唤醒的线程又可以抢到锁了，但是此时任务队列为空了，该线程会一直在while(taskSize_.load()==0)中循环，然后释放锁，继续阻塞在wait_for处，另外几个线程也要依次走过这个流程。
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
            // std::cout << "taskSize_: " << taskSize_ << std::endl;
            // 如果队列中还有任务，通知其他线程来处理任务
            if (taskSize_.load() > 0) {
                notEmptyCondition_.notify_all();
            }
            // 通知可以继续提交任务
            notFullCondition_.notify_all();
        }

        --idleThreadSize_;
        std::cout << "idleThreadSize_: " << idleThreadSize_ << std::endl;
        if (task)
            task->excute();
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

ThreadPool::~ThreadPool() {
    isRunning_ = false;

    std::unique_lock<std::mutex> lock(mutex_);
    // notEmptyCondition_.notify_all尽可能的往后靠，在线程的退出时有用
    notEmptyCondition_.notify_all();
    exitCondition_.wait(lock, [&] { return threads_.empty(); });
}

void ThreadPool::start(uint8_t threadnum) {
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

Result ThreadPool::submitTask(const std::shared_ptr<Task> &task) {
    // 获取锁
    // 线程通信，等待任务队列有空余位置
    // 如果有空余了，就把任务放入队列
    // 放入成功以后，在notEmpty_信号量上通知
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!notFullCondition_.wait_for(lock,
                                        std::chrono::seconds(1),
                                        [&] { return taskSize_.load() < taskQueueThreshold_; })) {
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
    return Result(task);
}
