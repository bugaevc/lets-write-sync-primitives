#undef NDEBUG

#include "barrier.h"
#include <vector>
#include <thread>
#include <cassert>

int main() {
    constexpr size_t num_threads = 100;
    unsigned char data[num_threads] = { 0 };
    std::vector<std::thread> threads;
    Barrier barrier { num_threads };

    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([i, &data, &barrier] {
            data[i] = 1;
            barrier.check_in_and_wait();
            for (size_t j = 0; j < num_threads; j++) {
                assert(data[j] == 1);
            }
        });
    }
    barrier.wait();
    for (size_t j = 0; j < num_threads; j++) {
        assert(data[j] == 1);
    }
    for (std::thread &thread : threads) {
        thread.join();
    }
}
