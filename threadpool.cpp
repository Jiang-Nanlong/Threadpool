//
// Created by cml on 24-12-11.
//
#include "threadpool.h"

#include <iostream>
#include <utility>

void Semaphore::wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock,[&]{return count_>0;});
    --count_;
}

void Semaphore::signal() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++count_;
    condition_.notify_all();
}

Result::Result(std::shared_ptr<Task> task, bool isValid):
    task_(std::move(task)),
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

Task::Task():result_(nullptr) {}

void Task::excute() {
    if (result_) {
        result_->setAny(std::move(run()));
    }
}

void Task::setResult(Result *res) {
    result_=res;
}

Thread::Thread(const func &func) {
    func_ = func;
}

void Thread::start() {
    std::thread t(func_);
    t.detach();
}

void ThreadPool::threadFunc() {
    // std::cout<<"ThreadPool::threadFunc(), this thread id: "<<std::this_thread::get_id()<<std::endl;
    for (;;) {
        // std::cout<<"thread: "<<std::this_thread::get_id()<<"已就绪"<<std::endl;
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            notEmptyCondition_.wait(lock,[&]{return !tasks_.empty();});

            task=tasks_.front();
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
        // std::cout<<"thread: "<<std::this_thread::get_id()<<"已完成"<<std::endl;
    }
}

ThreadPool::ThreadPool():
    threadSize_(0),
    taskSize_(0),
    taskQueueThreshold_(4),
    poolMode_(PoolMode::MODE_FIXED){
}

void ThreadPool::start(int threadnum) {
    threadSize_ = threadnum;

    for (int i = 0; i < threadSize_; ++i) {
        auto ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr));
    }

    for (int i = 0; i < threadSize_; ++i) {
        threads_[i]->start();
    }
}

void ThreadPool::setMode(const PoolMode& mode) {
    poolMode_ = mode;
}

void ThreadPool::setTaskQueueThreshold(int taskQueueThreshold) {
    taskQueueThreshold_ = taskQueueThreshold;
}

Result ThreadPool::submitTask(const std::shared_ptr<Task>& task) {
    // 获取锁
    // 线程通信，等待任务队列有空余位置
    // 如果有空余了，就把任务放入队列
    // 放入成功以后，在notEmpty_信号量上通知
    std::unique_lock<std::mutex> lock(mutex_);
    if (!notFullCondition_.wait_for(lock,
        std::chrono::seconds(1),
        [this]{return tasks_.size() < taskQueueThreshold_;})) {
        // 到时间结束也没有满足条件
        std::cerr<<"ThreadPool::submitTask() failed"<<std::endl;
        return Result(task, false);
    }
    tasks_.emplace(task);
    ++taskSize_;

    notEmptyCondition_.notify_all();
    return Result(task);
}


