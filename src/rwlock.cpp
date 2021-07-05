#include "rwlock.h"
#include "futex.h"
#include "util.h"
#include <climits>
#include <cassert>

void RWLock::lock_read() {
    uint32_t state2 = state.load(std::memory_order_relaxed);

    while (true) {
        // Disallow new readers when there are waiting writers.
        if (LIKELY(!(state2 & (locked_write_bit | need_to_wake_bit)))) {
            // Nobody is writing or waiting to write, let's attempt
            // to take the lock.
            uint32_t count = state2;
            uint32_t desired = count + 1;
            bool have_exchanged = state.compare_exchange_strong(
                state2, desired,
                std::memory_order_acquire, std::memory_order_relaxed
            );
            if (UNLIKELY(!have_exchanged)) {
                // Reevaluate.
                continue;
            }
            return;
        }
        // We're going to wait, so record the fact that we're waiting.
        if (!(state2 & need_to_wake_bit)) {
            assert(state2 == locked_write_bit);
            uint32_t desired = locked_write_bit | need_to_wake_bit;
            bool have_exchanged = state.compare_exchange_weak(
                state2, desired, std::memory_order_relaxed
            );
            if (UNLIKELY(!have_exchanged)) {
                // Reevaluate.
                continue;
            }
            state2 = desired;
        }
        futex_wait_bitset(
            (const uint32_t *) &state, state2, nullptr, reader_mask
        );
        // If somebody has woken up readers, we expect to see a 0.
        state2 = 0;
    }
}

void RWLock::lock_write() {
    uint32_t state2 = 0;
    bool have_exchanged = state.compare_exchange_strong(
       state2, locked_write_bit,
       std::memory_order_acquire, std::memory_order_relaxed
    );

    if (LIKELY(have_exchanged)) {
        return;
    }

    // Alrigth, the fast way didn't work, let's try the slow way.
    while (true) {
        if ((state2 & ~need_to_wake_bit) == 0) {
            assert(state2 == 0);
            // Try to grab it.
            have_exchanged = state.compare_exchange_strong(
               state2, locked_write_bit | need_to_wake_bit,
               std::memory_order_acquire, std::memory_order_relaxed
            );
            if (UNLIKELY(!have_exchanged)) {
                // Reevaluate.
                continue;
            }
            return;
        }
        // We're going to wait, so record the fact that we're waiting.
        if (!(state2 & need_to_wake_bit)) {
            uint32_t desired = state2 | need_to_wake_bit;
            have_exchanged = state.compare_exchange_strong(
                state2, desired, std::memory_order_relaxed
            );
            if (UNLIKELY(!have_exchanged)) {
                continue;
            }
            state2 = desired;
        }
        futex_wait_bitset(
            (const uint32_t *) &state, state2, nullptr, writer_mask
        );
        // If somebody has woken up a writer, we expect to see a 0 or a
        // need_to_wake_bit. Let's try guessing 0.
        state2 = 0;
    }
}

bool RWLock::try_lock_read() {
    uint32_t state2 = state.load(std::memory_order_relaxed);
    if (UNLIKELY(state2 & (locked_write_bit | need_to_wake_bit))) {
        return false;
    }
    uint32_t count = state2;
    uint32_t desired = count + 1;
    bool have_exchanged = state.compare_exchange_strong(
        state2, desired,
        std::memory_order_acquire, std::memory_order_relaxed
    );
    return LIKELY(have_exchanged);
}

bool RWLock::try_lock_write() {
    uint32_t expected = 0;
    bool have_locked = state.compare_exchange_strong(
       expected, locked_write_bit,
       std::memory_order_acquire, std::memory_order_relaxed
    );
    return have_locked;
}

bool RWLock::try_upgrade() {
    uint32_t state2 = 1;
    uint32_t desired = locked_write_bit;
    bool have_exchanged = state.compare_exchange_strong(
        state2, desired,
        std::memory_order_acquire, std::memory_order_relaxed
    );
    assert(!(state2 & locked_write_bit));
    if (have_exchanged) {
        return true;
    }
    if (state2 == (1 | need_to_wake_bit)) {
        // We can handle this situation too.
        // No new readers (or writers) can enter the critical
        // section if there are writers waiting.
        state2 = state.exchange(
            locked_write_bit | need_to_wake_bit,
            std::memory_order_acquire
        );
        assert(state2 == (1 | need_to_wake_bit));
        return true;
    }
    return false;
}

void RWLock::downgrade() {
    uint32_t state2 = state.exchange(1, std::memory_order_release);
    assert(state2 & locked_write_bit);
    uint32_t count = state2 & ~need_to_wake_bit & ~locked_write_bit;
    assert(count == 0);
    (void) count;
    if (UNLIKELY(state2 & need_to_wake_bit)) {
        // Wake all the readers.
        futex_wake_bitset((const uint32_t *) &state, INT_MAX, reader_mask);
    }
}

void RWLock::unlock_read() {
    uint32_t state2 = state.fetch_sub(1, std::memory_order_release);
    assert(!(state2 & locked_write_bit));
    // Note that state2 is the value of state pre-decrement here.
    uint32_t count = state2 & ~need_to_wake_bit;
    assert(count != 0);
    if (UNLIKELY(count == 1 && (state2 & need_to_wake_bit))) {
        // Wake one writer.
        state2 = need_to_wake_bit;
        state.compare_exchange_weak(state2, 0, std::memory_order_relaxed);
        futex_wake_bitset((const uint32_t *) &state, 1, writer_mask);
    }
}

void RWLock::unlock_write() {
    uint32_t state2 = state.exchange(0, std::memory_order_release);
    assert(state2 & locked_write_bit);
    uint32_t count = state2 & ~need_to_wake_bit & ~locked_write_bit;
    assert(count == 0);
    (void) count;
    if (UNLIKELY(state2 & need_to_wake_bit)) {
        // Wake all the readers and one writer.
        futex_wake_bitset((const uint32_t *) &state, INT_MAX, reader_mask);
        futex_wake_bitset((const uint32_t *) &state, 1, writer_mask);
    }
}
