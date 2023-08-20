
#include "TrWindowApp.h"
#include "TrD3D12Renderer/TrD3D12RendererBase.h"

HWND TrWindowApp::mHwnd = nullptr;


int TrWindowApp::Run(TrD3D12RendererBase* pRenderer, HINSTANCE hInstance, int nCmdShow)
{
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
		static_cast<LONG>(pRenderer->GetWidth()),
		static_cast<LONG>(pRenderer->GetHeight()),
		nullptr,
		nullptr,
		hInstance,
		pRenderer);

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
	TrD3D12RendererBase* pRenderer = reinterpret_cast<TrD3D12RendererBase*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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
		case WM_PAINT:
		{
			if (pRenderer)
			{
				pRenderer->OnUpdate();
			}
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
