#pragma once
#include "TrD3D12Util.h"

class TrD3D12RendererBase;

class TrWindowApp
{
public:

    static int Run(TrD3D12RendererBase* renderer, HINSTANCE hInstance, int nCmdShow);

    static HWND GetHwnd();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam); // WPARAM and LPARAM stand for word parameter and long parameter

private:
    static HWND mHwnd; // shared among all instances of the class
};

