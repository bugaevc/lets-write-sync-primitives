#include <atomic>

class Spinlock {
public:
    void lock();
    bool try_lock();
    void unlock();

private:
    std::atomic_bool locked { false };
};
