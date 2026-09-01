#pragma once
#include "Utilities/TrUtil.h"

class TrRendererBase;

class TrWindowApp
{
public:

    static int Run(TrRendererBase* renderer, HINSTANCE hInstance, int nCmdShow);

    static HWND GetHwnd();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam); // WPARAM and LPARAM stand for word parameter and long parameter

private:
    static HWND mHwnd; // shared among all instances of the class
    static UINT mPendingWidth;
    static UINT mPendingHeight;
    static bool mResizePending;
    static bool mMinimized;
};
