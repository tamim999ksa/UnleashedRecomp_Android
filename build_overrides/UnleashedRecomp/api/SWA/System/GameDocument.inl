namespace SWA
{
    inline Hedgehog::Base::TSynchronizedPtr<CGameDocument> CGameDocument::GetInstance()
    {
        return static_cast<CGameDocument*>(*(xpointer<CGameDocument>*)MmGetHostAddress(0x83367900));
    }
}
