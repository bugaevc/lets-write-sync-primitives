#undef NDEBUG

#include "spinlock.h"
#include "barrier.h"
#include <vector>
#include <thread>
#include <cassert>

int main() {
    constexpr size_t num_threads = 100;
    constexpr size_t num_times = 100;
    std::vector<int> v;
    std::vector<std::thread> threads;
    Spinlock spinlock;
    Barrier barrier { num_threads };

    // Uncontended.
    spinlock.lock();
    v.push_back(35);
    spinlock.unlock();
    assert(v.size() == 1);

    // Contended.
    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([&v, &barrier, &spinlock] {
            barrier.check_in_and_wait();
            for (size_t j = 0; j < num_times; j++) {
                spinlock.lock();
                v.push_back(35);
                spinlock.unlock();
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    assert(v.size() == 1 + num_times * num_threads);
}
