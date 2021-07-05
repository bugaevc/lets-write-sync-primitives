#include <atomic>
#include <cstddef>

class RWLock {
public:
    void lock_read();
    bool try_lock_read();
    void unlock_read();

    void lock_write();
    bool try_lock_write();
    void unlock_write();

    bool try_upgrade();
    void downgrade();

private:
    constexpr static uint32_t need_to_wake_bit = 1 << 31;
    constexpr static uint32_t locked_write_bit = 1 << 30;
    constexpr static uint32_t reader_mask = 1;
    constexpr static uint32_t writer_mask = 2;
    std::atomic_uint32_t state { 0 };
};
