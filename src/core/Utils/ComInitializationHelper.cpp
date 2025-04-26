#include <Objbase.h>
bool InitializeCom() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr);
}

void UninitializeCom() {
    CoUninitialize();
}
