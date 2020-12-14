#include <atomic>
#include <functional>

class Mutex;

class CondVar {
public:
    CondVar(Mutex &mutex);

    void wait();
    void wait(std::function<bool()> condition);

    void notify_one();
    void notify_all();

private:
    constexpr static uint32_t need_to_wake_one_bit = 1;
    constexpr static uint32_t need_to_wake_all_bit = 2;
    constexpr static uint32_t increment = 4;
    Mutex &mutex;
    std::atomic_uint32_t state { 0 };
};
