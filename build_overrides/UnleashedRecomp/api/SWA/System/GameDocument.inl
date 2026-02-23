namespace SWA
{
    inline Hedgehog::Base::TSynchronizedPtr<CGameDocument> CGameDocument::GetInstance()
    {
        return Hedgehog::Base::TSynchronizedPtr<CGameDocument>(*(xpointer<CGameDocument>*)MmGetHostAddress(0x83367900));
    }
}
