#pragma once
// We include the real one or mock it?
// Real one depends on other things. Let's mock TSynchronizedPtr to verify compilation of the *return type signature*.
// Wait, we need to verify the *GameDocument.inl* code compiles too.
// The .inl uses TSynchronizedPtr constructor.
// Let's try to use minimal implementation of TSynchronizedPtr.

namespace Hedgehog::Base
{
    template<typename T, bool ForceSync = false>
    class TSynchronizedPtr
    {
    public:
        T* m_pObject;
        TSynchronizedPtr(T* in_pObject) : m_pObject(in_pObject) {}
        TSynchronizedPtr() : m_pObject(nullptr) {}

        T* operator->() const { return m_pObject; }
        explicit operator bool() const { return m_pObject != nullptr; }
    };

    class CSynchronizedObject {};

    class CSharedString {};
}
