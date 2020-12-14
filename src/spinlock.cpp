#include "spinlock.h"
#include "util.h"
#include <sched.h>

void Spinlock::lock() {
    bool was_locked;
    int times = 0;
    do {
        if (UNLIKELY(times++ > 8)) {
            sched_yield();
        }
        was_locked = locked.exchange(true, std::memory_order_acquire);
    } while (UNLIKELY(was_locked));
}

bool Spinlock::try_lock() {
    bool was_locked = locked.exchange(true, std::memory_order_acquire);
    return LIKELY(!was_locked);
}

void Spinlock::unlock() {
    locked.store(false, std::memory_order_release);
}
