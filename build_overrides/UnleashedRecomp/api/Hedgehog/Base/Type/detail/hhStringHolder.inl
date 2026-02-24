namespace Hedgehog::Base
{
    inline SStringHolder* SStringHolder::GetHolder(const char* in_pStr)
    {
        return (SStringHolder*)((size_t)in_pStr - sizeof(RefCount));
    }

    inline SStringHolder* SStringHolder::Make(const char* in_pStr)
    {
        const size_t len = strlen(in_pStr);
        if (len > 0xFFFFFFFF - (sizeof(RefCount) + 1))
            return nullptr;

        auto pHolder = (SStringHolder*)__HH_ALLOC((uint32_t)(sizeof(RefCount) + len + 1));
        if (!pHolder)
            return nullptr;

        pHolder->RefCount = 1;
        strcpy(pHolder->aStr, in_pStr);
        return pHolder;
    }

    inline void SStringHolder::AddRef()
    {
        uint32_t originalValue = RefCount.value;
        be<uint32_t> original;
        be<uint32_t> incremented;
        do
        {
            original.value = originalValue;
            incremented = original + 1;
        } while (!__atomic_compare_exchange_n(&RefCount.value, &originalValue, incremented.value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    }

    inline void SStringHolder::Release()
    {
        uint32_t originalValue = RefCount.value;
        be<uint32_t> original;
        be<uint32_t> decremented;
        do
        {
            original.value = originalValue;
            decremented = original - 1;
        } while (!__atomic_compare_exchange_n(&RefCount.value, &originalValue, decremented.value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));

        if (decremented == 0)
            __HH_FREE(this);
    }

    inline bool SStringHolder::IsUnique() const
    {
        return RefCount == 1;
    }
}
