#ifndef NDEBUG
    #include <stdlib.h>
    #define UNREACHABLE() abort()
#else
    #define UNREACHABLE() __builtin_unreachable()
#endif

#define EXPECT(v, v2) __builtin_expect((v), (v2))
#define LIKELY(cond) __builtin_expect(!!(cond), 1)
#define UNLIKELY(cond) __builtin_expect(!!(cond), 0)

#if __cplusplus >= 201703L
    #define SUPPORTS_STRONGER_FAILURE_ORDERING 1
#endif

// ThreadSanitizer doesn't handle failure ordering properly yet.
// See https://github.com/google/sanitizers/issues/970
#ifdef __SANITIZE_THREAD__
    #undef SUPPORTS_STRONGER_FAILURE_ORDERING
#endif
#ifdef __has_feature
    #if __has_feature(thread_sanitizer)
        #undef SUPPORTS_STRONGER_FAILURE_ORDERING
    #endif
#endif
