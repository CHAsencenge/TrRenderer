// TrRenderer.cpp : 定义应用程序的入口点。
//

#include "pch.h"
// todo: define macros to isolate unused codes in precompile phase
#include "TrSoftwareRenderer/framework.h"
#include "TrRenderer.h"
#include "TrSoftwareRenderer/Window.h"
#include "TrSoftwareRenderer/Maths.h"
#include "TrSoftwareRenderer/Transform.h"
#include "TrSoftwareRenderer/Camera.h"
#include "TrSoftwareRenderer/TGA.h"
#include "TrSoftwareRenderer/Model.h"
#include "TrSoftwareRenderer/Shader.h"
#include "TrSoftwareRenderer/ClassUtils.h"
#include <iostream>
#include <vector>

#include "TrD3D12Renderer/TrD3D12Util.h"



// 相机，自身位置eye，目标位置target，向上向量up

extern Mat<4, 4> ModelView;
extern Mat<4, 4> Projection;
extern Mat<4, 4> ReversedZ;
extern Mat<4, 4> Viewport;

// Configuration Property---Linker---System---SubSystem---Console
int main(int argc, char* argv[])
{
    std::cout << "main" << std::endl;
    int width = 1200, height = 1200;
    window_init(width, height, L"TrRenderer");
    /*while (1)
    {
        std::cout << "console hello" << std::endl;
    }*/
    std::cout << argv << std::endl;

    // 静态图生成
    // 创建一个TGAImage类型的frame buffer
    TGAImage framebuf(width, height, 4);
    // build model view projection矩阵
    // 
    // 初始化相机参数，创建相机
    float fovY = PI / 3.0f;
    float aspect = 4.0f / 3.0f;
    float e[] = { 0, 0.9f, 0.45f };
    float t[] = { 0, 0, 0 };
    float u[] = { 0, 1, 0 };
    Camera DefaultCam(e, t, u, fovY, aspect);
    DefaultCam.SetMatLookAt();
    ModelView = DefaultCam.GetMatLookAt();
    TrUtils::PrintMat(ModelView, "main ModelView");

    ReversedZ = std::vector<Vec<4>>{Vec<4>(1,0,0,0), Vec<4>(0,1,0,0), Vec<4>(0,0,-1,0), Vec<4>(0,0,1,1)};


    float nearplane = -0.5f;
    float farplane = -10.0f;
    Projection = Mat4Perspective(DefaultCam.fovY, DefaultCam.aspect, abs(nearplane), abs(farplane));
    
    // 创建一个 reversed z-buffer
    std::vector<std::vector<float>> zbuf (width, std::vector<float>(height, 0.0f));
    float** zbuf1 = new float*[width];
    for(int i = 0; i < width; i++)
    {
        zbuf1[i] = new float[height];
    }
    
    
    // 遍历读取输入的obj，创建model和shader
    // obj\\boggie\\body.obj
    Model model("obj\\boggie\\body.obj");
    float ld[] = { 1, 1, 1 };
    Vec3 lightDir(ld);
    DefaultShader shader(model, lightDir, zbuf, zbuf1);

    // 遍历所有的faces
    for(int i = 0; i < model.GetNumFaces(); i++)
    { 
        // vertex shader算出的顶点的clip space坐标会写入到这里，相当于gl_Position
        Vec<4> clipVert[3];
        // 对三角形的每个顶点调用顶点着色器
        // face unit结构 v1 / vt1 / vn1    v2 / vt2 / vn2    v3 / vt3 / vn3
        std::vector<int> faceVertsIdx = model.FaceVert(i);
        for (int j = 0; j < faceVertsIdx.size(); j++)
        {
            shader.VertexShader(i, j, clipVert[j]);
        }
        
        // 光栅化
        Triangle(clipVert, shader, framebuf);
    }

    // 绘制到窗口
    window_draw_bgra(framebuf.GetRawBGRAData());
    
    // 用创建的frame buffer将结果写入tga文件保存
    bool bWriteTGAFile = framebuf.WriteTGAFile("output.tga", false, false);

    
    while (1)
    {
        Sleep(500);
    }
    return 0;
}



// -----------------------------------------------------------------------------------------

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


/* note:
 * Unicode: uses multiple bytes to present characters
 * supports characters from various scripts and languages (Latin, Chinese, Arabic)
 */

// Configuration Property---Linker---System---SubSystem---Window
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。
    std::cout << "wWinMain" << std::endl;

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TRRENDERER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TRRENDERER));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        std::cout << "hello" << std::endl;
    }

    return (int) msg.wParam;
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRRENDERER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TRRENDERER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// -----------------------------------------------------------------------------------------

/* note:
* ANSI: uses a single byte to present each character
* ANSI character set is based on the ASCII character set (english, digits, some special)
* ANSI encoding is limited in its ability to represent non-Latin characters
 */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    std::cout << "WinMain" << std::endl;
    
    // use hInstance to create a Renderer inst

    // Renderer initialize

    // Renderer main loop

    // catch exception and return
}
