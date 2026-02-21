#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

// Include the Mutex wrapper
// The Mutex wrapper uses std::mutex on non-Windows platforms, which requires <mutex>.
// The wrapper itself doesn't include <mutex> in the #else branch, so we must include it here.
#include "../mutex.h"

TEST_CASE("Mutex Basic Lock/Unlock") {
    Mutex m;
    m.lock();
    m.unlock();
}

TEST_CASE("Mutex Mutual Exclusion") {
    Mutex m;
    int shared_resource = 0;
    const int num_threads = 10;
    const int increments_per_thread = 10000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                m.lock();
                shared_resource++;
                m.unlock();
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    CHECK(shared_resource == num_threads * increments_per_thread);
}

TEST_CASE("Mutex RAII Compatibility") {
    Mutex m;

    SUBCASE("std::lock_guard") {
        std::lock_guard<Mutex> lock(m);
        CHECK(true); // Just ensure it compiles and runs without crashing
    }

    SUBCASE("std::unique_lock") {
        std::unique_lock<Mutex> lock(m);
        CHECK(lock.owns_lock());
        lock.unlock();
        CHECK(!lock.owns_lock());
        lock.lock();
        CHECK(lock.owns_lock());
    }
}
