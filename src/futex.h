#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstdint>

struct timespec;

static inline int futex_wait(
    const uint32_t *uaddr, int val, struct timespec *timeout
) {
    return syscall(SYS_futex, uaddr, FUTEX_WAIT_PRIVATE, val, timeout);
}

static inline int futex_wake(const uint32_t *uaddr, int number) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE_PRIVATE, number);
}

static inline int futex_wait_bitset(
    const uint32_t *uaddr, int val, struct timespec *timeout, uint32_t mask
) {
    return syscall(
        SYS_futex, uaddr, FUTEX_WAIT_BITSET_PRIVATE, val, timeout, 0, mask
   );
}

static inline int futex_wake_bitset(
    const uint32_t *uaddr, int number, uint32_t mask
) {
    return syscall(
        SYS_futex, uaddr, FUTEX_WAKE_BITSET_PRIVATE, number, 0, 0, mask
    );
}

static inline int futex_requeue(
    const uint32_t *uaddr, int number_to_wake,
    const uint32_t *uaadr2, int number_to_requeue
) {
    return syscall(
        SYS_futex, uaddr, FUTEX_REQUEUE_PRIVATE,
        number_to_wake, number_to_requeue, uaadr2
   );
}
