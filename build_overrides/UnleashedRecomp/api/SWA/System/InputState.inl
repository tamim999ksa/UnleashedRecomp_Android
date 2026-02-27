namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CInputState>
    inline Hedgehog::Base::TSynchronizedPtr<CInputState> CInputState::GetInstance()
    {
        return Hedgehog::Base::TSynchronizedPtr<CInputState>((*(xpointer<CInputState>*)MmGetHostAddress(0x83361F5C)));
    }

    inline const SPadState& CInputState::GetPadState() const
    {
        return m_PadStates[m_CurrentPadStateIndex];
    }
}
