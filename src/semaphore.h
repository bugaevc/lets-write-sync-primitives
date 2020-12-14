#include <atomic>
#include <cstddef>

class Semaphore {
public:
    Semaphore(size_t initial_value);
    void down();
    bool try_down();
    void up();

private:
    constexpr static uint32_t need_to_wake_bit = 1 << 31;
    std::atomic_uint32_t state;
};
