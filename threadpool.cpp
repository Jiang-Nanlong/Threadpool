//
// Created by cml on 24-12-11.
//
#include "threadpool.h"

#include <iostream>

Thread::Thread(func f) {
    func_  = f;
}

void Thread::start() {
    std::thread t(func_);
    t.detach();
}

void ThreadPool::threadFunc() {
    std::cout<<"ThreadPool::threadFunc(), this thread id: "<<std::this_thread::get_id()<<std::endl;

}

ThreadPool::ThreadPool():
    threadSize_(0),
    taskSize_(0),
    taskQueueThreshold_(1024),
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

void ThreadPool::submitTask(std::shared_ptr<Task> task) {
    //获取锁
    // 线程通信，等待任务队列有空余位置
    //如果有空余了，就把任务放入队列
    //放入成功以后，在notEmpty_信号量上通知
    std::unique_lock<std::mutex> lock(mutex_);
    if (!notFullCondition_.wait_for(lock,
        std::chrono::seconds(1),
        [this]{return tasks_.size() < taskQueueThreshold_;})) {
        // 到时间结束也没有满足条件
        std::cerr<<"ThreadPool::submitTask() failed"<<std::endl;
        return;
    }
    tasks_.emplace(task);
    ++taskSize_;

    notEmptyCondition_.notify_all();

}


