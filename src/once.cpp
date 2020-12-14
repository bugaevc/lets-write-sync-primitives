#include "once.h"
#include "futex.h"
#include "util.h"
#include <climits>

void Once::perform(std::function<void ()> callback) {
    uint32_t state2 = INITIAL;
    bool have_exchanged = state.compare_exchange_strong(
        state2, PERFORMING_NO_WAITERS,
#ifdef SUPPORTS_STRONGER_FAILURE_ORDERING
        std::memory_order_relaxed,
#endif
        std::memory_order_acquire
    );

    if (UNLIKELY(have_exchanged)) {
        // We saw INITIAL and changed it to PERFORMING_NO_WAITERS;
        // we should perform the operation now.
        callback();
        // Now, record that we're done.
        state2 = state.exchange(DONE, std::memory_order_release);
        switch (EXPECT(state2, PERFORMING_NO_WAITERS)) {
        case PERFORMING_NO_WAITERS:
            // Nothing to do!
            break;
        case PERFORMING:
            // Wake everyone who's waiting for us.
            futex_wake((const uint32_t *) &state, INT_MAX);
            break;
        default:
            UNREACHABLE();
            break;
        }
        // We're all done here!
        return;
    }

    while (true) {
        // Alright, let's see what the state is (was).
        switch (EXPECT(state2, DONE)) {
        case DONE:
            // Awesome, nothing to do then.
            return;
        case PERFORMING_NO_WAITERS:
            have_exchanged = state.compare_exchange_weak(
                state2, PERFORMING,
#ifdef SUPPORTS_STRONGER_FAILURE_ORDERING
                std::memory_order_relaxed,
#endif
                std::memory_order_acquire
            );
            if (UNLIKELY(!have_exchanged)) {
                // Something has changed already,
                // reevaluate without waiting.
                continue;
            }
            state2 = PERFORMING;
            // Fallthrough.
        case PERFORMING:
            // Let's wait for it.
            futex_wait((const uint32_t *) &state, state2, nullptr);
            // We have been woken up, but that might
            // have been spurious. Reevaluate.
            state2 = state.load(std::memory_order_acquire);
            continue;
        default:
            UNREACHABLE();
            break;
        }
    }
}
