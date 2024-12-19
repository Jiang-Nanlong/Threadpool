#include <iostream>
#include <numeric>

#include "threadpool.h"

#define TEST

#ifdef TEST
class test : public Task {
    int left;
    int right;

public:
    test(int left, int right): left(left), right(right) {
    }

    Any run() override {
        std::cout << "test start: =======" << std::endl;
        // uint64_t sum = 0;
        // for (int i = left; i <= right; i++) {
        //     sum += i;
        // }
        // return sum;
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        return "";
    }
};
#endif

int main() {
    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);

#ifdef TEST
    Result re1 = pool.submitTask(std::make_shared<test>(1, 100000000));
    Result re2 = pool.submitTask(std::make_shared<test>(100000001, 200000000));
    Result re3 = pool.submitTask(std::make_shared<test>(200000001, 300000000));
    Result re4 = pool.submitTask(std::make_shared<test>(300000001, 400000000));
    Result re5 = pool.submitTask(std::make_shared<test>(300000001, 400000000));
    Result re6 = pool.submitTask(std::make_shared<test>(300000001, 400000000));

    // ulong res1 = re1.get().cast_<ulong>();
    // ulong res2 = re2.get().cast_<ulong>();
    // ulong res3 = re3.get().cast_<ulong>();
    // ulong res4 = re4.get().cast_<ulong>();
    // ulong res5 = re5.get().cast_<ulong>();
    // ulong res6 = re6.get().cast_<ulong>();
    // std::cout << (res1 + res2 + res3 + res4 + res5 + res6) << std::endl;

#endif
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
