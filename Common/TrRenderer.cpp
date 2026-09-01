// TrRenderer.cpp : 定义应用程序的入口点。
//


//#include "pch.h"
// todo: define macros to isolate unused codes in precompile phase

#include "TrRenderer.h"
#include "TrLog.h"

#ifdef IS_TR_SOFTWARE_RENDERER
#include "../TrSoftwareRenderer/framework.h"
#include "../TrSoftwareRenderer/Window.h"
#include "../TrSoftwareRenderer/Maths.h"
#include "../TrSoftwareRenderer/Transform.h"
#include "../TrSoftwareRenderer/Camera.h"
#include "../TrSoftwareRenderer/TGA.h"
#include "../TrSoftwareRenderer/Model.h"
#include "../TrSoftwareRenderer/Shader.h"
#include "../TrSoftwareRenderer/ClassUtils.h"
#endif

#ifdef IS_TR_VULKAN_RENDERER
#include "../TrVulkanRenderer/TrVulkanRendererRaster.h"
#include "../TrVulkanRenderer/TrVulkanRendererRayTracing.h"
#endif


#ifdef IS_TR_SOFTWARE_RENDERER
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
    Model model(SLN_ROOT_DIR "Models/objs/boggie/body.obj");
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
#endif

// -----------------------------------------------------------------------------------------

/* note:
* ANSI: uses a single byte to present each character
* ANSI character set is based on the ASCII character set (english, digits, some special)
* ANSI encoding is limited in its ability to represent non-Latin characters
 */

#ifdef IS_TR_VULKAN_RENDERER
int main()
{
    // TrVulkanRendererRaster VulkanRenderer(1920, 1080, "TrVulkan");
    TrVulkanRendererRayTracingBase VulkanRenderer(1920, 1080, "TrVulkan");

    try
    {
        VulkanRenderer.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif
