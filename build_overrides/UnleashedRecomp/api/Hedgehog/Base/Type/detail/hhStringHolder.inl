namespace Hedgehog::Base
{
    inline SStringHolder* SStringHolder::GetHolder(const char* in_pStr)
    {
        return (SStringHolder*)((size_t)in_pStr - sizeof(RefCount));
    }

    inline SStringHolder* SStringHolder::Make(const char* in_pStr, size_t length)
    {
        constexpr size_t overhead = sizeof(RefCount) + 1;

        if (length > UINT32_MAX - overhead)
            return nullptr;

        auto pHolder = (SStringHolder*)__HH_ALLOC((uint32_t)(overhead + length));

        if (!pHolder)
            return nullptr;

        pHolder->RefCount = 1;
        memcpy(pHolder->aStr, in_pStr, length);
        pHolder->aStr[length] = '\0';
        return pHolder;
    }

    inline void SStringHolder::AddRef()
    {
        std::atomic_ref refCount(RefCount.value);

        be<uint32_t> original, incremented;
        do
        {
            original = RefCount;
            incremented = original + 1;
        } while (!refCount.compare_exchange_weak(original.value, incremented.value));
    }

    inline void SStringHolder::Release()
    {
        std::atomic_ref refCount(RefCount.value);

        be<uint32_t> original, decremented;
        do
        {
            original = RefCount;
            decremented = original - 1;
        } while (!refCount.compare_exchange_weak(original.value, decremented.value));

        if (decremented == 0)
            __HH_FREE(this);
    }

    inline bool SStringHolder::IsUnique() const
    {
        return RefCount == 1;
    }
}
