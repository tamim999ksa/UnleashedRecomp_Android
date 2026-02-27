#pragma once
namespace boost {
    template<typename T>
    struct shared_ptr {
        T* ptr;
        T* operator->() const { return ptr; }
        T& operator*() const { return *ptr; }
    };
}
