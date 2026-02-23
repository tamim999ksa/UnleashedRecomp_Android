namespace SWA
{
    inline Hedgehog::Base::TSynchronizedPtr<CApplicationDocument> CApplicationDocument::GetInstance()
    {
        return Hedgehog::Base::TSynchronizedPtr<CApplicationDocument>(static_cast<CApplicationDocument*>(*(xpointer<CApplicationDocument>*)MmGetHostAddress(0x833678A0)));
    }
}
