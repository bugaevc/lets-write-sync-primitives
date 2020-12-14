#undef NDEBUG

#include "event.h"
#include <vector>
#include <thread>
#include <cassert>

int main() {
    std::vector<int> v;
    Event event;

    std::thread reader { [&v, &event] {
        event.wait();
        assert(v.size() == 1);
    } };

    std::thread writer { [&v, &event] {
        sched_yield();
        v.push_back(35);
        event.notify();
    } };

    reader.join();
    writer.join();
}
