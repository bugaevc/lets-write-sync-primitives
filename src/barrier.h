#include <atomic>
#include <cstddef>

class Barrier {
public:
    Barrier(size_t required);

    void check_in();
    void wait();

    bool try_wait();
    void check_in_and_wait();
    bool check_in_and_try_wait();

private:
    constexpr static uint32_t need_to_wake_bit = 1 << 31;
    std::atomic_uint32_t state;
};
