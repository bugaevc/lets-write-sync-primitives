#include "barrier.h"
#include "futex.h"
#include "util.h"
#include <climits>

Barrier::Barrier(size_t required)
    : state(required) { }

void Barrier::check_in() {
    uint32_t state2 = state.fetch_sub(1, std::memory_order_release) - 1;
    if (UNLIKELY(state2 == need_to_wake_bit)) {
        state.store(0, std::memory_order_relaxed);
        futex_wake((const uint32_t *) &state, INT_MAX);
    }
}

void Barrier::wait() {
    uint32_t state2 = state.load(std::memory_order_acquire);
    while (UNLIKELY(state2 & ~need_to_wake_bit)) {
        if (!(state2 & need_to_wake_bit)) {
            bool have_exchanged = state.compare_exchange_weak(
                state2, state2 | need_to_wake_bit,
#ifdef SUPPORTS_STRONGER_FAILURE_ORDERING
                std::memory_order_relaxed,
#endif
                std::memory_order_acquire
            );
            if (UNLIKELY(!have_exchanged)) {
                continue;
            }
            state2 |= need_to_wake_bit;
        }
        futex_wait((const uint32_t *) &state, state2, nullptr);
        state2 = state.load(std::memory_order_acquire);
    }
}

bool Barrier::try_wait() {
    uint32_t state2 = state.load(std::memory_order_acquire);
    return LIKELY(!(state2 & ~need_to_wake_bit));
}

void Barrier::check_in_and_wait() {
    uint32_t state2 = state.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (!(state2 & ~need_to_wake_bit)) {
        // We saw 0 after we decremented it ourselves.
        if (UNLIKELY(state2 == need_to_wake_bit)) {
            state.store(0, std::memory_order_relaxed);
            futex_wake((const uint32_t *) &state, INT_MAX);
        }
        return;
    }
    // From here on, if we see 0, it is because of somebody else,
    // so we just have to wake up, and not wake somebody else up.
    do {
        if (!(state2 & need_to_wake_bit)) {
            bool have_exchanged = state.compare_exchange_weak(
                state2, state2 | need_to_wake_bit,
#ifdef SUPPORTS_STRONGER_FAILURE_ORDERING
                std::memory_order_relaxed,
#endif
                std::memory_order_acquire
            );
            if (UNLIKELY(!have_exchanged)) {
                continue;
            }
            state2 |= need_to_wake_bit;
        }
        futex_wait((const uint32_t *) &state, state2, nullptr);
        state2 = state.load(std::memory_order_acquire);
    } while (UNLIKELY(state2 & ~need_to_wake_bit));
}

bool Barrier::check_in_and_try_wait() {
    uint32_t state2 = state.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (LIKELY(state2 & ~need_to_wake_bit)) {
        return false;
    }
    if (UNLIKELY(state2 == need_to_wake_bit)) {
        state.store(0, std::memory_order_relaxed);
        futex_wake((const uint32_t *) &state, INT_MAX);
    }
    return true;
}
