lib_sync_primitives = library('sync_primitives',
    'mutex.h',
    'mutex.cpp',

    'once.h',
    'once.cpp',

    'barrier.h',
    'barrier.cpp',

    'spinlock.h',
    'spinlock.cpp',

    'event.h',
    'event.cpp',

    'semaphore.h',
    'semaphore.cpp',

    'rwlock.h',
    'rwlock.cpp',

    'condvar.h',
    'condvar.cpp',
)
sync_primitives = declare_dependency(
    link_with: lib_sync_primitives,
    include_directories: '.'
)
