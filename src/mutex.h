#include <atomic>

class Mutex {
public:
    void lock();
    bool try_lock();
    void unlock();

private:
    friend class CondVar;
    void lock_pessimistic();

    enum {
        UNLOCKED,
        LOCKED_NO_NEED_TO_WAKE,
        LOCKED_NEED_TO_WAKE,
    };
    std::atomic_uint32_t state { UNLOCKED };
};
