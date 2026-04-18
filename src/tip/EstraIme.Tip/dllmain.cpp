#include "TipGuids.h"

#include <Windows.h>
#include <objbase.h>

HRESULT CreateClassFactory(REFCLSID clsid, REFIID riid, void** ppvObject);
HRESULT RegisterServer(HMODULE moduleHandle);
HRESULT UnregisterServer();

HMODULE g_moduleHandle = nullptr;

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_moduleHandle = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv)
{
    return CreateClassFactory(clsid, riid, ppv);
}

STDAPI DllRegisterServer()
{
    const auto hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto hr = RegisterServer(g_moduleHandle);
    if (SUCCEEDED(hrInit))
    {
        CoUninitialize();
    }
    return hr;
}

STDAPI DllUnregisterServer()
{
    const auto hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto hr = UnregisterServer();
    if (SUCCEEDED(hrInit))
    {
        CoUninitialize();
    }
    return hr;
}
