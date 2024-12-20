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
        uint64_t sum = 0;
        for (int i = left; i <= right; i++) {
            sum += i;
        }
        return sum;
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        return "";
    }
};
#endif

int main() {
#ifdef TEST
    {
#endif

        ThreadPool pool;
        // pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);

#ifdef TEST
        Result re1 = pool.submitTask(std::make_shared<test>(1, 100000000));
        Result re2 = pool.submitTask(std::make_shared<test>(100000001, 200000000));
        Result re3 = pool.submitTask(std::make_shared<test>(200000001, 300000000));
        Result re4 = pool.submitTask(std::make_shared<test>(300000001, 400000000));
        Result re5 = pool.submitTask(std::make_shared<test>(300000001, 400000000));
        Result re6 = pool.submitTask(std::make_shared<test>(300000001, 400000000));

        uint64_t res1 = re1.get().cast_<uint64_t>();
        uint64_t res2 = re2.get().cast_<uint64_t>();
        uint64_t res3 = re3.get().cast_<uint64_t>();
        uint64_t res4 = re4.get().cast_<uint64_t>();
        uint64_t res5 = re5.get().cast_<uint64_t>();
        uint64_t res6 = re6.get().cast_<uint64_t>();
        std::cout << (res1 + res2 + res3 + res4 + res5 + res6) << std::endl;
        // std::cout << res1 << std::endl;
    }
#endif
    std::cout << "main over" << std::endl;
    // getchar();
    return 0;
}
