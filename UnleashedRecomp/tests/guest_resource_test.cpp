#include "../gpu/guest_resource.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>

void test_basic_ref_counting() {
    GuestResource res(ResourceType::Texture);
    assert(res.refCount == 1);

    res.AddRef();
    assert(res.refCount == 2);

    res.Release();
    assert(res.refCount == 1);

    std::cout << "test_basic_ref_counting passed" << std::endl;
}

void test_concurrency() {
    GuestResource res(ResourceType::Texture);
    int num_threads = 10;
    int iterations = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&res, iterations]() {
            for (int j = 0; j < iterations; ++j) {
                res.AddRef();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    assert(res.refCount == 1 + num_threads * iterations);

    threads.clear();
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&res, iterations]() {
            for (int j = 0; j < iterations; ++j) {
                res.Release();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    assert(res.refCount == 1);
    std::cout << "test_concurrency passed" << std::endl;
}

int main() {
    try {
        test_basic_ref_counting();
        test_concurrency();
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
