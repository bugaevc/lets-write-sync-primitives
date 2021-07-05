#include "semaphore.h"
#include "futex.h"
#include "util.h"

Semaphore::Semaphore(size_t initial_value)
    : state(initial_value) { }

void Semaphore::down() {
    uint32_t state2 = state.load(std::memory_order_relaxed);
    bool responsible_for_waking = false;

    while (true) {
        // Note that the state2 value is just a speculation here.
        uint32_t count = state2 & ~need_to_wake_bit;
        if (LIKELY(count > 0)) {
            // It looks like there are some free slots.
            uint32_t perhaps_wake_bit = state2 & need_to_wake_bit;
            bool going_to_wake = false;
            if (UNLIKELY(responsible_for_waking && !perhaps_wake_bit)) {
                // If we have ourselves been woken up previously, and the
                // need_to_wake_bit is not set, that means some more slots might
                // be available now, and it's us who has to wake up additional
                // threads.
                if (UNLIKELY(count > 1)) {
                    going_to_wake = true;
                }
                // Set the need_to_wake_bit. This means that waking further
                // threads the next time will be the responsibility of up()
                // calls, not of down() calls; in particular not of down()
                // calls in the threads we're about to wake.
                perhaps_wake_bit = need_to_wake_bit;
            }
            uint32_t desired = (count - 1) | perhaps_wake_bit;
            bool have_exchanged = state.compare_exchange_weak(
                state2, desired,
                std::memory_order_acquire, std::memory_order_relaxed
            );
            if (UNLIKELY(!have_exchanged)) {
                // Reevaluate.
                continue;
            }
            if (UNLIKELY(going_to_wake)) {
                futex_wake((const uint32_t *) &state, count - 1);
            }
            return;
        }
        // We're probably going to sleep, so attempt to set the need to wake
        // bit. We do not commit to sleeping yet, though, as setting the bit
        // may fail and cause us to reevaluate what we're doing.
        if (state2 == 0) {
            bool have_exchanged = state.compare_exchange_weak(
                state2, need_to_wake_bit, std::memory_order_relaxed
            );
            if (UNLIKELY(!have_exchanged)) {
                // Reevaluate.
                continue;
            }
            state2 = need_to_wake_bit;
        }
        responsible_for_waking = true;
        futex_wait((const uint32_t *) &state, state2, nullptr);
        // This is the state we will probably see upon being waked:
        state2 = 1;
        // If we guess this wrong, the compare_exchange() above
        // will fail, we'll load the actual value and reevaluate.
    }
}

bool Semaphore::try_down() {
    uint32_t state2 = state.load(std::memory_order_relaxed);
    uint32_t count = state2 & ~need_to_wake_bit;
    if (count == 0) {
        return false;
    }
    uint32_t desired = (count - 1) | (state2 & need_to_wake_bit);
    bool have_exchanged = state.compare_exchange_strong(
        state2, desired,
        std::memory_order_acquire, std::memory_order_relaxed
    );
    return LIKELY(have_exchanged);
}

void Semaphore::up() {
    uint32_t state2 = state.fetch_add(1, std::memory_order_release);
    if (LIKELY(!(state2 & need_to_wake_bit))) {
        return;
    }
    // Clear the need_to_wake_bit; the thread we will wake below becomes
    // responsible for waking others if further slots become available later.
    state2 = state.fetch_and(~need_to_wake_bit, std::memory_order_relaxed);
    if (UNLIKELY(!(state2 & need_to_wake_bit))) {
        // Someone else has handled it already.
        return;
    }
    futex_wake((const uint32_t *) &state, 1);
}
