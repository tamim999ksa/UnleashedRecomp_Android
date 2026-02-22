namespace Hedgehog::Base
{
    inline SStringHolder* SStringHolder::GetHolder(const char* in_pStr)
    {
        return (SStringHolder*)((size_t)in_pStr - sizeof(RefCount));
    }

    inline SStringHolder* SStringHolder::Make(const char* in_pStr)
    {
        auto pHolder = (SStringHolder*)__HH_ALLOC(sizeof(RefCount) + strlen(in_pStr) + 1);
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
