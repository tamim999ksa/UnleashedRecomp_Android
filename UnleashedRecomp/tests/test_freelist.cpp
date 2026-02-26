#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <vector>
#include <memory>
#include "kernel/freelist.h"

// Helper struct to track construction and destruction
struct TestItem {
    int id;
    bool* destroyed;

    TestItem() : id(0), destroyed(nullptr) {}

    // Constructor with args for emplace checks if needed, but FreeList::Alloc calls construct_at or emplace_back with no args by default
    // Wait, FreeList::Alloc calls std::construct_at(&items[idx]) which default constructs, or items.emplace_back() which default constructs.

    ~TestItem() {
        if (destroyed) {
            *destroyed = true;
        }
    }
};

TEST_CASE("FreeList Allocation and Deallocation") {
    FreeList<int> fl;

    SUBCASE("Initial Allocation") {
        size_t idx1 = fl.Alloc();
        CHECK(idx1 == 0);
        CHECK(fl.items.size() == 1);

        size_t idx2 = fl.Alloc();
        CHECK(idx2 == 1);
        CHECK(fl.items.size() == 2);
    }

    SUBCASE("Freeing by Index and Reuse") {
        size_t idx1 = fl.Alloc();
        size_t idx2 = fl.Alloc();
        size_t idx3 = fl.Alloc();

        fl.Free(idx2);
        CHECK(fl.freed.size() == 1);
        CHECK(fl.freed.back() == idx2);

        // Reuse freed slot
        size_t idx4 = fl.Alloc();
        CHECK(idx4 == idx2);
        CHECK(fl.freed.empty());

        // Allocate new slot
        size_t idx5 = fl.Alloc();
        CHECK(idx5 == 3);
    }

    SUBCASE("Freeing by Reference") {
        size_t idx1 = fl.Alloc();
        size_t idx2 = fl.Alloc();

        int& item1 = fl[idx1];
        fl.Free(item1);

        CHECK(fl.freed.size() == 1);
        CHECK(fl.freed.back() == idx1);

        // Reuse
        size_t idx3 = fl.Alloc();
        CHECK(idx3 == idx1);
    }
}

TEST_CASE("FreeList Object Lifecycle") {
    FreeList<TestItem> fl;
    bool destroyed1 = false;
    bool destroyed2 = false;

    size_t idx1 = fl.Alloc();
    fl[idx1].id = 100;
    fl[idx1].destroyed = &destroyed1;

    size_t idx2 = fl.Alloc();
    fl[idx2].id = 200;
    fl[idx2].destroyed = &destroyed2;

    SUBCASE("Destruction on Free(size_t)") {
        fl.Free(idx1);
        CHECK(destroyed1 == true);
        CHECK(fl.freed.size() == 1);
        CHECK(fl.freed.back() == idx1);
    }

    SUBCASE("Destruction on Free(T&)") {
        fl.Free(fl[idx2]);
        CHECK(destroyed2 == true);
        CHECK(fl.freed.size() == 1);
        CHECK(fl.freed.back() == idx2);
    }

    SUBCASE("Reconstruction on Alloc") {
        fl.Free(idx1);
        destroyed1 = false; // Reset tracking

        size_t idx3 = fl.Alloc();
        CHECK(idx3 == idx1);
        // The object should be reconstructed.
        // FreeList::Alloc calls std::construct_at(&items[idx])
        // Since TestItem default constructor sets id=0 and destroyed=nullptr
        CHECK(fl[idx3].id == 0);
        CHECK(fl[idx3].destroyed == nullptr);
    }
}

TEST_CASE("FreeList LIFO Reuse") {
    FreeList<int> fl;
    size_t idx1 = fl.Alloc();
    size_t idx2 = fl.Alloc();
    size_t idx3 = fl.Alloc();

    fl.Free(idx1);
    fl.Free(idx2);

    // Expect LIFO order: idx2 then idx1
    CHECK(fl.Alloc() == idx2);
    CHECK(fl.Alloc() == idx1);
}
