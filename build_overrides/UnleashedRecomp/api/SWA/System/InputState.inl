namespace SWA
{
    inline Hedgehog::Base::TSynchronizedPtr<CInputState> CInputState::GetInstance()
    {
        return static_cast<CInputState*>(*(xpointer<CInputState>*)MmGetHostAddress(0x83361F5C));
    }

    inline const SPadState& CInputState::GetPadState() const
    {
        return m_PadStates[m_CurrentPadStateIndex];
    }
}
