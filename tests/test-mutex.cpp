#undef NDEBUG

#include "mutex.h"
#include <vector>
#include <thread>
#include <sched.h>
#include <cassert>

int main() {
    constexpr size_t num_threads = 100;
    constexpr size_t num_times = 100;
    std::vector<int> v;
    std::vector<std::thread> threads;
    Mutex mutex;

    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([&v, &mutex] {
            for (size_t j = 0; j < num_times; j++) {
                mutex.lock();
                v.push_back(35);
                sched_yield();
                mutex.unlock();
                sched_yield();
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    assert(v.size() == num_times * num_threads);
    assert(mutex.try_lock());
    assert(!mutex.try_lock());
}
