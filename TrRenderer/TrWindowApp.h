
#include "TrD3D12Renderer/TrD3D12Renderer.h"

class TrWindowApp
{
public:
    TrWindowApp();
    ~TrWindowApp();
    TrWindowApp(TrWindowApp& other) = delete;
    TrWindowApp& operator=(TrWindowApp& other) = delete;

    void Initialize();

    static HWND GetHwnd();
};

