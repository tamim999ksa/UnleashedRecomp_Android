#pragma once

namespace Hedgehog::Base
{
    template<class T>
    struct TAllocator
    {
        using value_type = T;

        TAllocator() noexcept {}
        template<class U> TAllocator(TAllocator<U> const&) noexcept {}

        value_type* allocate(std::size_t n)
        {
            return new value_type[n];
        }

        void deallocate(value_type* p, std::size_t) noexcept
        {
            delete[] p;
        }
    };
}
