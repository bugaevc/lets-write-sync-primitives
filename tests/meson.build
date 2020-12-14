threads = dependency('threads')

all_tests = [
    'mutex',
    'once',
    'spinlock',
    'event',
    'semaphore',
    'rwlock',
    'barrier',
]

foreach name : all_tests
    test_name = 'test-' + name
    exe = executable(test_name,
        test_name + '.cpp',
        dependencies: [sync_primitives, threads]
    )
    test(test_name, exe)
endforeach
