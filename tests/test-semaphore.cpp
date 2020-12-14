#undef NDEBUG

#include "semaphore.h"
#include "barrier.h"
#include <vector>
#include <thread>
#include <sched.h>
#include <unistd.h>
#include <cassert>

constexpr static size_t num_threads = 100;
constexpr static size_t num_times = 100;

void lock_test() {
    std::vector<int> v;
    std::vector<std::thread> threads;
    Semaphore semaphore { 1 };

    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([&v, &semaphore] {
            for (size_t j = 0; j < num_times; j++) {
                semaphore.down();
                v.push_back(35);
                sched_yield();
                semaphore.up();
                sched_yield();
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    assert(v.size() == num_times * num_threads);

    v.clear();
    assert(semaphore.try_down());
    assert(!semaphore.try_down());
}

void event_test() {
    std::vector<int> v;
    Semaphore semaphore { 0 };

    std::thread reader { [&v, &semaphore] {
        semaphore.down();
        assert(v.size() == 1);
    } };

    std::thread writer { [&v, &semaphore] {
        sched_yield();
        v.push_back(35);
        semaphore.up();
    } };

    writer.join();
    reader.join();

    assert(!semaphore.try_down());
}

void nonbinary_test() {
    constexpr size_t num = 5;
    std::vector<std::thread> threads;
    Semaphore semaphore { num };
    Barrier barrier { num_threads };
    std::atomic_uint32_t value { 0 };
    std::atomic_bool seen_more_than_two { false };

    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([&] {
            barrier.check_in_and_wait();
            for (size_t j = 0; j < num_times; j++) {
                semaphore.down();
                uint32_t v = 1 + value.fetch_add(1, std::memory_order_relaxed);
                assert(v <= num);
                if (v > 2) {
                    seen_more_than_two.store(true, std::memory_order_relaxed);
                }
                sched_yield();
                value.fetch_sub(1, std::memory_order_relaxed);
                semaphore.up();
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }
    assert(value.load(std::memory_order_relaxed) == 0);
    assert(seen_more_than_two.load(std::memory_order_relaxed));
    for (size_t  i = 0; i < num; i++) {
        assert(semaphore.try_down());
    }
    assert(!semaphore.try_down());
}

int main() {
    lock_test();
    event_test();
    nonbinary_test();
}
