#include <iostream>

#include "threadpool.h"

//#define TEST

#ifdef TEST
class test:public Task {
public:
    void run() override {
        std::cout << "test" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
};
#endif

int main() {
    ThreadPool pool;
    pool.start(4);

#ifdef TEST
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());
    pool.submitTask(std::make_shared<test>());

    getchar();
#endif
    return 0;
}
