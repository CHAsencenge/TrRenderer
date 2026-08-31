
#include "TrWindowApp.h"
#include "TrRendererBase.h"

HWND TrWindowApp::mHwnd = nullptr;
UINT TrWindowApp::mPendingWidth = 0;
UINT TrWindowApp::mPendingHeight = 0;
bool TrWindowApp::mResizePending = false;
bool TrWindowApp::mMinimized = false;


int TrWindowApp::Run(TrRendererBase* pRenderer, HINSTANCE hInstance, int nCmdShow)
{
	mResizePending = false;
	mMinimized = false;

	// parse the command line parameters
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	pRenderer->ParseCommandLineArgs(argv, argc);
	LocalFree(argv);

	// initialize the window class
	WNDCLASSEX wndClass = {0};
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WindowProc;
	wndClass.hInstance = hInstance;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpszClassName = L"TrRenderer";
	RegisterClassEx(&wndClass);

	RECT wndRect = { 0, 0, static_cast<LONG>(pRenderer->GetWidth()), static_cast<LONG>(pRenderer->GetHeight()) };
	AdjustWindowRect(&wndRect, WS_OVERLAPPEDWINDOW, FALSE);

	// create the window and store a handle to it
	mHwnd = CreateWindow(
		wndClass.lpszClassName,
		pRenderer->GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wndRect.right - wndRect.left,
		wndRect.bottom - wndRect.top,
		nullptr,
		nullptr,
		hInstance,
		pRenderer);

	RECT clientRect = {};
	GetClientRect(mHwnd, &clientRect);
	pRenderer->OnResize(
		static_cast<UINT>(clientRect.right - clientRect.left),
		static_cast<UINT>(clientRect.bottom - clientRect.top));
	mResizePending = false;

	// initialize the renderer
	pRenderer->OnInitialize();

	ShowWindow(mHwnd, nCmdShow);

	// main loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else if (mResizePending && !mMinimized)
		{
			const UINT width = mPendingWidth;
			const UINT height = mPendingHeight;
			mResizePending = false;
			pRenderer->OnResize(width, height);
		}
		else if (!mMinimized)
		{
			pRenderer->OnUpdate();
			pRenderer->OnRender();
		}
		else
		{
			WaitMessage();
		}
	}

	pRenderer->OnDestroy();

	// return quit message to window
	return static_cast<char>(msg.wParam);
}

HWND TrWindowApp::GetHwnd()
{
	return mHwnd;
}

// must be static for wndClass.lpfnWndProc
LRESULT CALLBACK TrWindowApp::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// each window has a long ptr, to store a custom data
	// GWLP_USERDATA is an index to access this user data
	TrRendererBase* pRenderer = reinterpret_cast<TrRendererBase*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE:
		{
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			return 0;
		}
		case WM_KEYDOWN:
		{
			if (pRenderer)
			{
				pRenderer->OnKeyDown(static_cast<UINT8>(wParam));
			}
			return 0;
		}
		case WM_KEYUP:
		{
			if (pRenderer)
			{
				pRenderer->OnKeyUp(static_cast<UINT8>(wParam));
			}
			return 0;
		}
		case WM_SIZE:
		{
			mMinimized = wParam == SIZE_MINIMIZED;
			if (!mMinimized)
			{
				mPendingWidth = LOWORD(lParam);
				mPendingHeight = HIWORD(lParam);
				mResizePending = mPendingWidth != 0 && mPendingHeight != 0;
			}
			else
			{
				mResizePending = false;
			}
			return 0;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT paintStruct = {};
			BeginPaint(hWnd, &paintStruct);
			EndPaint(hWnd, &paintStruct);
			return 0;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
