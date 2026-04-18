#include "TextService.h"
#include "TipGuids.h"

#include <atomic>
#include <winrt/base.h>

namespace
{
    class TextServiceFactory final : public IClassFactory
    {
    public:
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (!ppvObject)
            {
                return E_INVALIDARG;
            }
            if (riid == IID_IUnknown || riid == IID_IClassFactory)
            {
                *ppvObject = static_cast<IClassFactory*>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override
        {
            return ++refCount_;
        }

        STDMETHODIMP_(ULONG) Release() override
        {
            const auto value = --refCount_;
            if (value == 0)
            {
                delete this;
            }
            return value;
        }

        STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppvObject) override
        {
            if (outer)
            {
                return CLASS_E_NOAGGREGATION;
            }

            auto* service = new EstraIme::Tip::TextService();
            const auto hr = service->QueryInterface(riid, ppvObject);
            service->Release();
            return hr;
        }

        STDMETHODIMP LockServer(BOOL) override
        {
            return S_OK;
        }

    private:
        std::atomic<ULONG> refCount_{1};
    };
}

HRESULT CreateClassFactory(REFCLSID clsid, REFIID riid, void** ppvObject)
{
    if (clsid != CLSID_EstraImeTextService)
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new TextServiceFactory();
    const auto hr = factory->QueryInterface(riid, ppvObject);
    factory->Release();
    return hr;
}
