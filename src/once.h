#include <atomic>
#include <functional>

class Once {
public:
    void perform(std::function<void()> callback);

private:
    enum {
        INITIAL,
        DONE,
        PERFORMING_NO_WAITERS,
        PERFORMING,
    };
    std::atomic_uint32_t state { INITIAL };
};
