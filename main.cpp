#include <iostream>
#include <numeric>

#include "threadpool.h"

#define TEST

int sum1(int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return a + b;
}

int sum2(int a, int b, int c) {
    std::this_thread::sleep_for(std::chrono::seconds(5));

    return a + b + c;
}

int sum3(int a, int b, int c, int d) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return a + b + c + d;
}


int main() {
#ifdef TEST
    {
#endif

        ThreadPool &pool = ThreadPool::getInstance();
        // pool.setMode(PoolMode::MODE_CACHED);
        pool.start(2);

#ifdef TEST
        auto res1 = pool.submitTask(sum1, 1, 1);
        auto res2 = pool.submitTask(sum2, 2, 2, 2);
        auto res3 = pool.submitTask(sum3, 3, 3, 3, 3);
        auto res4 = pool.submitTask(sum3, 3, 3, 3, 3);
        auto res5 = pool.submitTask(sum3, 3, 3, 3, 3);

        std::cout << res1.get() << std::endl;
        std::cout << res2.get() << std::endl;
        std::cout << res3.get() << std::endl;
        std::cout << res4.get() << std::endl;
        std::cout << res5.get() << std::endl;
    }
#endif
    std::cout << "main over" << std::endl;
    // getchar();
    return 0;
}
