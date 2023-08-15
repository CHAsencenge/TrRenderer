

#include "pch.h"
#include "TrWindowApp.h"

HWND TrWindowApp::mHwnd = nullptr;


int TrWindowApp::Initialize(TrD3D12RendererBase* renderer, HINSTANCE hInstance, int nCmdShow)
{
	// parse the command line parameters
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	renderer->ParseCommandLineArgs(argv, argc);
	LocalFree(argv);

	// initialize the window class


	// create the windwo and store a handle to it

	// initialize the renderer

	// main loop

	// return quit message to window
}

HWND TrWindowApp::GetHwnd()
{
	return mHwnd;
}

LRESULT TrWindowApp::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	return LRESULT();
}
