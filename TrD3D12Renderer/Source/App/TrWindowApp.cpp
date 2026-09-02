
#include "TrWindowApp.h"
#include "TrRendererBase.h"
#include "imgui.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

HWND TrWindowApp::mHwnd = nullptr;
UINT TrWindowApp::mPendingWidth = 0;
UINT TrWindowApp::mPendingHeight = 0;
bool TrWindowApp::mResizePending = false;
bool TrWindowApp::mMinimized = false;


int TrWindowApp::Run(
	TrRendererBase* pRenderer,
	HINSTANCE hInstance,
	int nCmdShow,
	const std::optional<std::wstring>& sceneOverride)
{
	mResizePending = false;
	mMinimized = false;

	// parse the command line parameters
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	pRenderer->ParseCommandLineArgs(argv, argc);
	LocalFree(argv);
	if(sceneOverride.has_value())
	{
		pRenderer->SetScenePath(*sceneOverride);
	}

	// initialize the window class
	WNDCLASSEX wndClass = {0};
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WindowProc;
	wndClass.hInstance = hInstance;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpszClassName = L"TrRenderer";
	RegisterClassEx(&wndClass);

	constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	RECT wndRect = { 0, 0, static_cast<LONG>(pRenderer->GetWidth()), static_cast<LONG>(pRenderer->GetHeight()) };
	AdjustWindowRect(&wndRect, windowStyle, FALSE);

	// create the window and store a handle to it
	mHwnd = CreateWindow(
		wndClass.lpszClassName,
		pRenderer->GetTitle(),
		windowStyle,
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
	mHwnd = nullptr;

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
	const LRESULT imguiResult =
		ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
	const bool imguiWantsKeyboard =
		ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
	const bool imguiWantsTextInput =
		ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
	const bool imguiWantsMouse =
		ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;

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
			const bool isCameraMovementKey =
				wParam == 'W' || wParam == 'A' || wParam == 'S' || wParam == 'D';
			const bool rendererMayUseKey =
				!imguiWantsKeyboard ||
				(isCameraMovementKey && !imguiWantsTextInput);
			if (pRenderer && rendererMayUseKey)
			{
				pRenderer->OnKeyDown(static_cast<UINT8>(wParam));
			}
			return 0;
		}
		case WM_MOUSEMOVE:
		{
			if(pRenderer)
			{
				pRenderer->OnMouseMove(
					static_cast<INT>(static_cast<short>(LOWORD(lParam))),
					static_cast<INT>(static_cast<short>(HIWORD(lParam))));
			}
			return 0;
		}
		case WM_RBUTTONDOWN:
		{
			if(pRenderer && !imguiWantsMouse)
			{
				pRenderer->OnRightMouseButtonDown(
					static_cast<INT>(static_cast<short>(LOWORD(lParam))),
					static_cast<INT>(static_cast<short>(HIWORD(lParam))));
			}
			return 0;
		}
		case WM_RBUTTONUP:
		case WM_CAPTURECHANGED:
		{
			if(pRenderer)
			{
				pRenderer->OnRightMouseButtonUp();
			}
			return 0;
		}
		case WM_KILLFOCUS:
		{
			if(pRenderer)
			{
				pRenderer->OnInputFocusLost();
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
	if(imguiResult != 0)
	{
		return imguiResult;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
