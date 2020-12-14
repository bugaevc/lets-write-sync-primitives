#undef NDEBUG

#include "once.h"
#include "barrier.h"
#include <vector>
#include <thread>
#include <cassert>

int main() {
    constexpr size_t num_threads = 100;
    constexpr size_t num_times = 100;
    std::vector<int> v;
    std::vector<std::thread> threads;
    Barrier barrier { num_threads };
    Once once1, once2[num_times];

    // Uncontended when not done.
    once1.perform([&v] {
        v.push_back(35);
    });
    assert(v.size() == 1);

    // Contended.
    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([&v, &barrier, &once2] {
            barrier.check_in_and_wait();
            for (size_t j = 0; j < num_times; j++) {
                once2[j].perform([&v] {
                    v.push_back(35);
                });
                once2[j].perform([] {
                    assert(false);
                });
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    assert(v.size() == 1 + num_times);

    // Uncontended when done.
    once2[0].perform([&v] {
        v.push_back(35);
    });
    assert(v.size() == 1 + num_times);
}
