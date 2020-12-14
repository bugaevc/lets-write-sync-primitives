#include "event.h"
#include "futex.h"
#include "util.h"
#include <climits>

void Event::notify() {
    uint32_t state2 = state.exchange(SET, std::memory_order_release);
    switch (EXPECT(state2, UNSET)) {
    case UNSET_NO_WAITERS:
    case SET:
        break;
    case UNSET:
        futex_wake((const uint32_t *) &state, INT_MAX);
        break;
    }
}

void Event::wait() {
    uint32_t state2 = UNSET_NO_WAITERS;
    bool have_exchanged = state.compare_exchange_strong(
        state2, UNSET,
#ifdef SUPPORTS_STRONGER_FAILURE_ORDERING
        std::memory_order_relaxed,
#endif
        std::memory_order_acquire
    );

    if (LIKELY(have_exchanged)) {
        // Update state2 to represent our expected
        // value of state. We know we've just set
        // state to UNSET, so...
        state2 = UNSET;
    }

    while (UNLIKELY(state2 != SET)) {
        futex_wait((const uint32_t *) &state, state2, nullptr);
        state2 = state.load(std::memory_order_acquire);
    }
}

bool Event::try_wait() {
    uint32_t state2 = state.load(std::memory_order_acquire);
    return state2 == SET;
}
