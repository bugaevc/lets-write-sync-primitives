# Let's write synchronization primitives!

This is a project for me to explore the internals of various synchronization
primitives, as well as generic patterns around atomics and futexes.

# Design decisions

All of the synchronization primitives are implemented using atomic variables,
and (with the exception of spinlock) futexes, as supported by the Linux kernel
and some other systems. All operations feature a *fast path*, which avoids
calling into the kernel for futex operations, and only uses atomics; and a slow
path which makes full use of futexes. In a way, each primitive actually contains
two different implementations, one based on atomics and one based on futexes,
and switches between them transparently when needed/possible.

The other idea that the primitives could benefit from, optimistic spinning
(meaning, transparently upgrading from a spinlock to a mutex), is intentionally
*not* implemented here. This means the code is somewhat simpler than it could
be, but it also means that this implementation is knowingly not as fast as it
could possibly be. After all, this is a learning project and not a
production-grade library.

Most of the blocking operations (such as `semaphore.down()` and `mutex.lock()`)
have a corresponding `try_xxx()` version that never blocks the calling thread,
and returns a boolean describing whether the operation could be (and have been)
performed immediately, without blocking. Semantically, a `try_xxx()` operation
succeeding has the same effect (with respect to happens-before relationships and
synchronization guarantees) as executing the corresponding `xxx()` operation,
and a `try_xxx()` operation failing has the same effect as if it has not been
executed at all. In either case, the calling thread is not blocked. It might be
a good idea to use the non-blocking operations whenever your thread has other
useful work to do that can be done without waiting for the other threads.

All the locks implemented here are not reentrant: a thread already holding the
lock cannot claim it again. In fact, none of the primitives track which thread
it is that is holding the lock.

The implementation of synchronization primitives includes some assertions. Those
have been quite helpful during development, but they add unnecessary overhead at
runtime. Define the standard `NDEBUG` macro to disable assertions (see below).

# Primitives

The following synchronization primitives are implemented.

## Spinlock

This is the only primitive here to not use futexes. Instead of sleeping
properly when it cannot acquire the lock, it just spins in a cycle.

## Mutex

A mutual exclusion lock. It has the same API as a spinlock, but uses a futex to
sleep when the lock cannot be taken immediately. If there's no contention,
locking the mutex succeeds immediately, without calling into the kernel. In the
same way, unlocking an uncontended mutex does not call into the kernel.

A mutex establishes a total order among executions of critical sections.
Everything written by one execution of a critical session will be seen by the
following ones.

A mutex can be seen as a special case of a semaphore, or as a special case of a
read-write lock. However, the mutex is faster than either, because of a far
simpler implementation.

## Event

An event primitive can be used to wait for some sort of event. Multiple threads
can wait for the event by calling `event.wait()`, and multiple threads can
concurrently announce the event by calling `event.notify()`. Calling
`event.wait()` is fast if the event has already been announced, and similarly
calling `event.notify()` is fast if nobody's waiting.

Note that many other event implementations additionally provide an
`event.clear()` method that "clears" the event, so that it can be waited for and
announced again. This implementation does not, because this usage pattern is
inherently racy. Use a condition variable for this.

Everything that has happened before the event was announced for the first time
will be seen after `event.wait()` call returns.

Both barriers and condition variables (and also semaphores) can all be regarded
as generalizations of events.

## Once

A once primitive can be used to execute a critical session only once, even if
multiple threads reach the critical section at the same time. This is typically
used for lazy initialization. The same effect can be trivially achieved by using
a mutex and a boolean variable, but a once primitive is more efficient. In
particular, once the critical section has been executed, subsequent calls of
`once.perform()` are very fast and don't block each other.

The once primitive establishes a happens-before relationship between the
completion of the critical session and the `once.perform()` calls returning.
After the call returns, the calling thread will see everything written by the
critical session.

## Barrier

A barrier is similar to an event, except a barrier waits for *several* threads
to "check in" (and unlike with an event, it's invalid to over-check-in). You
have to declare the number of required check-ins when constructing a barrier.

Here's a common way to use a barrier:

```cpp
Barrier barrier { num_threads };
for (size_t i = 0; i < num_threads; i++) {
    spawn_thread([&] {
        some_work_1();
        barrier.check_in();
        some_work_2();
        barrier.wait();
        some_work_3();
    });
}
```

Here, `some_work_3()` will see the results of `some_work_1()` calls (but not of
`some_work_2()` calls!) made by all the threads in the group. It's highly
beneficial to perform some (useful) work in between calling `barrier.check_in()`
and `barrier.wait()`, because the `barrier.check_in()` call will be very fast if
nobody is waiting, and similarly `barrier.wait()` will be very fast if everyone
has already checked in (so there's no need to actually wait). In case you have
absolutely nothing to do there, and do call `barrier.wait()` immediately after
calling `barrier.check_in()`, there's a combined `barrier.check_in_and_wait()`
call that is slightly faster than doing the two calls separately.

Note that waiting can be done from any thread, not just those that have checked
in. For example, a barrier can also be used in the following manner:

```cpp
Barrier barrier { num_threads };
for (size_t i = 0; i < num_threads; i++) {
    spawn_thread([&] {
        some_work_1();
        barrier.check_in();
    });
}
barrier.wait();
some_work_2();
```

Here, `some_work_2()` will see the results of `some_work_1()` of all threads.

## Readers-writer lock

A readers-writer lock is a generalization of a mutex. Either a single writer or
multiple readers can hold the lock at one time; readers do not block each other.
If most of the accesses are reads with only some occasional writes, it's
probably a good idea to use a readers-writer lock instead of a plain mutex; all
reader operations are very fast if no writers are involved. That being said, the
implementation is more complex and a bit less efficient than that of a plain
mutex.

There are two caveats to using a readers-writer lock (at least as implemented
here):
* The lock prefers writers over readers; if there are writers waiting to acquire
  the lock, newly arriving readers will not be allowed to take the lock. This
  means that the readers can't completely starve the writers, and everyone will
  get the lock eventually. But this also means that slow readers can actually
  block other readers.
* It is not possible to "upgrade" a held lock from reading to writing (meaning
  lock the lock for writing if you already hold it for reading). To see why,
  consider what would happen if that was allowed, and two readers both tried to
  upgrade the lock at the same time. Instead, you should drop the reading lock,
  and then re-acquire it for writing; and be prepared that something might have
  changed while you were not holding the lock.

A readers-writer lock establishes a happens-before relationship between a
writer unlocking the lock and a reader or a writer subsequently locking the
lock, as well as between a reader or a writer unlocking the lock and a writer
subsequently locking the lock. It does not, however, establish any
happens-before relationships between several readers locking and unlocking the
lock if there's no writer locking the lock in between them.

Here's an example of using a readers-writer lock to protect a rarely updated
value:

```cpp
std::string hostname;
RWLock lock;

std::string get_hostname() {
    lock.lock_read();
    std::string hostname_copy = hostname;
    lock.unlock_read();
    return hostname_copy;
}

void set_hostname(std::string new_hostname) {
    lock.lock_write();
    hostname = new_hostname;
    lock.unlock_write();
}
```

And here's an example of how to properly handle the case where you would want to
upgrade the lock:

```cpp
SomeCache cache;
RWLock lock;

value_t get(key_t key) {
    lock.lock_read();
    value_t value = cache[key];
    lock.unlock_read();

    if (value) {
        return value;
    }

    lock.lock_write();
    // Somebody might have already
    // put it there, so recheck.
    value = cache[key];
    if (!value) {
        value = cache[key] = calculate_value(key);
    }
    lock.unlock_write();
    return value;
}
```

## Semaphore

A semaphore is a different generalization of a mutex. A semaphore keeps an
internal counter which can be incremented with `semaphore.up()` and decremented
with `semaphore.down()`. The counter cannot become negative, so if the counter
is zero when `semaphore.down()` is called, it blocks until somebody else calls
`semaphore.up()` and thus allows the `semaphore.down()` call to proceed.

If the initial value of the counter is one, a semaphore functions like a mutex,
with `semaphore.down()` acting like `mutex.lock()`, and `semaphore.up()` acting
like `mutex.unlock()`. If the initial value is zero, a semaphore functions like
an event primitive (only supporting a single thread that waits and a single
thread that announces the event). A semaphore with a higher initial value can be
used to model a shared resource with a limited number of access slots.

A semaphore is by far the most complex of the primitives implemented here. You
should probably use a specialized synchronization primitive such as a mutex or
an event instead. That being said, both `semaphore.down()` and `semaphore.up()`
should be fast as long as no thread has to wait.

It's not very clear what happens-before relationships exactly a semaphore
establishes, but it should, at least, establish a happens-before relationship
between someone incrementing the counter from zero and someone subsequently
decrementing it. This implementation, in addition to that, establishes a
happens-before relationship between anyone incrementing the counter (not
necessarily from zero) and someone subsequently decrementing it.

## Condition variable

A condition variable can be seen as another generalization of the event
primitive. Unlike an event, a condition variable can be used to properly track a
condition that changes between being true and false, repeatedly. Also unlike an
event, a condition variable doesn't store whether the condition is true or false
internally; instead, it's up to the user to somehow store and check the
condition. The only requirement, as far as the condition variable is concerned,
is that the condition must only be checked and modified while holding a mutex;
and the condition variable must be given access to this mutex.

The `condvar.wait()` method must be called with the mutex held; it atomically
unlocks the mutex, and starts waiting for some other thread to notify this
thread (spurious wake-ups are allowed); after completing the wait it locks the
mutex again and returns to the caller with the mutex held, the caller should
then inspect the state. Waiting threads can be woken up with either
`condvar.notify_one()` (typically used for conditions that should be handled or
consumed by waiting threads, such as a queue of events that should be drained by
worker threads) or `condvar.notify_all()` (typically used for announcing events
that the waiting threads are not expected to consume). Neither
`condvar.notify_one()` nor `condvar.notify_all()` must be called with the mutex
held; although it's correct to call them while either holding or not holding the
mutex, it's much faster to call them without holding the mutex.

A condition variable itself does not establish any happens-before relationships.
However, it must be used with a mutex that does establish such relationships.

# Building

Let's write synchronization primitives is built with
[Meson](https://mesonbuild.com/). Here are a few useful build configurations:

* The default debug build:
  ```
  $ meson build
  ```
* Debug build with ThreadSanitizer enabled, to check for data races:
  ```
  $ meson build -Db_sanitize=thread
  ```
* Release build with static linking, optimizations, and disabled assertions:
  ```
  $ meson build -Dbuildtype=release -Ddefault_library=static -Db_lto=true -Db_ndebug=true
  ```

Use `ninja` to build and `ninja test` to run the tests.

# Resources

* [`futex(2)`](https://man7.org/linux/man-pages/man2/futex.2.html) and
  [`futex(7)`](https://man7.org/linux/man-pages/man7/futex.7.html) manual pages
* [Futexes Are Tricky](https://akkadia.org/drepper/futex.pdf)
* [Locking in WebKit](https://webkit.org/blog/6161/locking-in-webkit/)
* [`std::memory_order`](https://en.cppreference.com/w/cpp/atomic/memory_order)
* [Mutexes Are Faster Than Spinlocks](https://matklad.github.io/2020/01/04/mutexes-are-faster-than-spinlocks.html)
