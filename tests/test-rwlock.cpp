#undef NDEBUG

#include "rwlock.h"
#include "barrier.h"
#include <vector>
#include <thread>
#include <sched.h>
#include <cassert>

int main() {
    constexpr size_t num_threads = 100;
    constexpr size_t num_times = 100;
    constexpr size_t write_ratio = 10;
    std::vector<int> v;
    std::vector<std::thread> threads;
    RWLock rwlock;
    Barrier barrier { num_threads };

    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([i, &barrier, &v, &rwlock] {
            barrier.check_in_and_wait();
            for (size_t j = 0; j < num_times; j++) {
                if (i * write_ratio == j) {
                    rwlock.lock_write();
                    v.push_back(35);
                    sched_yield();
                    rwlock.unlock_write();
                } else {
                    rwlock.lock_read();
                    if (!v.empty()) {
                        assert(v.back() == 35);
                    }
                    sched_yield();
                    rwlock.unlock_read();
                }
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    assert(rwlock.try_lock_write());
    assert(!rwlock.try_lock_write());
    assert(!rwlock.try_lock_read());
    assert(v.size() == std::min(num_threads, num_times / write_ratio));
}
