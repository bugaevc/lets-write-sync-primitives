#include "condvar.h"
#include "mutex.h"
#include "futex.h"
#include "util.h"
#include <cstdint>
#include <climits>

CondVar::CondVar(Mutex &mutex)
    : mutex(mutex) { }

void CondVar::wait() {
    uint32_t state2 = state.fetch_or(
        need_to_wake_all_bit | need_to_wake_one_bit,
        std::memory_order_relaxed
    ) | need_to_wake_all_bit | need_to_wake_one_bit;
    mutex.unlock();
    futex_wait((const uint32_t *) &state, state2, nullptr);
    // Re-lock the mutex, but make sure to try to wake somebody up when we
    // unlock it. This is because notify_all() requeues a bunch of threads to
    // wait on the mutex without making them register with the mutex properly.
    // This is fine, as long as the one thread that it does wake itself commits
    // to waking somebody up on unlock, so that's what we do.
    mutex.lock_pessimistic();
}

void CondVar::wait(std::function<bool()> condition) {
    while (!condition()) {
        wait();
    }
}

void CondVar::notify_one() {
    uint32_t state2 = state.fetch_add(
        increment, std::memory_order_relaxed
    ) + increment;
    if (UNLIKELY(state2 & need_to_wake_one_bit)) {
        // Wake someone, and clear the need_to_wake_one_bit if there was nobody
        // for us to wake, to take the fast path the next time. Since we only
        // learn whether there has been somebody waiting or not after we have
        // tried to wake them, it would make sense for us to clear the bit after
        // trying to wake someone up and seeing there was nobody waiting; but
        // that would race with somebody else setting the bit. Therefore, we do
        // it like this: attempt to clear the bit first...
        state.compare_exchange_weak(
            state2, state2 & ~need_to_wake_one_bit,
            std::memory_order_relaxed
        );
        // ...try to wake someone...
        int woken = futex_wake((const uint32_t *) &state, 1);
        // ...and if we have woken someone, put the bit back.
        if (woken) {
            state.fetch_or(need_to_wake_one_bit, std::memory_order_relaxed);
        }
    }
}

void CondVar::notify_all() {
    uint32_t state2 = state.fetch_add(
        increment, std::memory_order_relaxed
    ) + increment;
    if (UNLIKELY(state2 & need_to_wake_all_bit)) {
        state.fetch_and(
            ~(need_to_wake_all_bit | need_to_wake_one_bit),
            std::memory_order_relaxed
        );
        futex_requeue(
            (const uint32_t *) &state, 1,
            (const uint32_t *) &mutex.state, INT_MAX
        );
    }
}
