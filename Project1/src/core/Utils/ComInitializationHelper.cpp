#include "ComInitializationHelper.h"

ComInitializationHelper::ComInitializationHelper()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == RPC_E_CHANGED_MODE)
    {
        // Already initialized with a different mode; handle accordingly
        // e.g. ignore or log
    }
}

ComInitializationHelper::~ComInitializationHelper()
{
    // Clean up COM
    CoUninitialize();
}
