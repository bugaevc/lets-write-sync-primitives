#include "mutex.h"
#include "futex.h"
#include "util.h"
#include <cstdint>

void Mutex::lock() {
    // Fast path: attempt to claim the mutex without waiting.
    uint32_t state2 = UNLOCKED;
    bool have_exchanged = state.compare_exchange_strong(
        state2, LOCKED_NO_NEED_TO_WAKE,
        std::memory_order_acquire, std::memory_order_relaxed
    );

    if (LIKELY(have_exchanged)) {
        // We grabbed the mutex the fast way, awesome!
        return;
    }

    // Important: the slow path *always* sets the state to LOCKED_NEED_TO_WAKE
    // (not LOCKED_NO_NEED_TO_WAKE), even if it observes the state being
    // UNLOCKED at some point. This is so that if a thread goes to sleep here
    // in lock(), it will always make sure to wake up the next one in line when
    // it gets to unlock(). Without this guarantee, waking just one sleeping
    // thread in unlock() would not be enough, since that wouldn't guarantee the
    // other sleeping threads will eventually get woken up, too.
    //
    // Note that LOCKED_NO_NEED_TO_WAKE does not necessarily imply that there
    // are no waiters, only that the thread holding the mutex is not responsible
    // for waking them up (perhaps some other thread is). In the same way,
    // LOCKED_NEED_TO_WAKE does not necessarily imply that there are waiters,
    // only that the thread holding the mutex is responsible for trying to wake
    // someone up (whether there is in fact someone to wake up or not).

    if (state2 != LOCKED_NEED_TO_WAKE) {
        state2 = state.exchange(LOCKED_NEED_TO_WAKE, std::memory_order_acquire);
    }

    while (UNLIKELY(state2 != UNLOCKED)) {
        futex_wait((const uint32_t *) &state, LOCKED_NEED_TO_WAKE, nullptr);
        state2 = state.exchange(LOCKED_NEED_TO_WAKE, std::memory_order_acquire);
    }
}

void Mutex::lock_pessimistic() {
    // Same as above, but do not even attempt to jump to LOCKED_NO_NEED_TO_WAKE.
    // This method is used by CondVar::wait(), see the comment there.
    uint32_t state2 = state.exchange(
        LOCKED_NEED_TO_WAKE, std::memory_order_acquire
    );

    while (UNLIKELY(state2 != UNLOCKED)) {
        futex_wait((const uint32_t *) &state, LOCKED_NEED_TO_WAKE, nullptr);
        state2 = state.exchange(LOCKED_NEED_TO_WAKE, std::memory_order_acquire);
    }
}

bool Mutex::try_lock() {
    uint32_t expected = UNLOCKED;
    bool have_locked = state.compare_exchange_strong(
        expected, LOCKED_NO_NEED_TO_WAKE,
        std::memory_order_acquire, std::memory_order_relaxed
    );
    return LIKELY(have_locked);
}

void Mutex::unlock() {
    uint32_t state2 = state.exchange(UNLOCKED, std::memory_order_release);
    switch (EXPECT(state2, LOCKED_NO_NEED_TO_WAKE)) {
    case UNLOCKED:
        UNREACHABLE();
        break;
    case LOCKED_NO_NEED_TO_WAKE:
        break;
    case LOCKED_NEED_TO_WAKE:
        // Wake just one thread up. Since the thread was sleeping, it has taken
        // the slow path in lock(), which means it'll eventually wake the next
        // thread up, and so on. This means we're fine here waking just one of
        // the threads and not all of them.
        futex_wake((const uint32_t *) &state, 1);
        break;
    }
}
