#include <atomic>

class Event {
public:
    void notify();
    void wait();
    bool try_wait();

private:
    enum {
        UNSET_NO_WAITERS,
        UNSET,
        SET,
    };
    std::atomic_uint32_t state { UNSET_NO_WAITERS };
};
