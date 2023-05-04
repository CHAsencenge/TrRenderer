#include "Shader.h"

extern Mat<4, 4> ModelView;
extern Mat<4, 4> Projection;

IShader::IShader()
{
}

IShader::~IShader()
{
}

// 用floot函数将float类型的采样点坐标向下取整
TGAColor IShader::Sample2D(TGAImage& img, Vec2 uv)
{
	return img.Get(floor(uv.e[0] * img.Width()), floor(uv.e[1] * img.Height()));
}


DefaultShader::DefaultShader(const Model& m, Vec3& lightDir) : model(m)
{
	uniformLightDir = (ModelView * lightDir.AddDimension1(0.0f)).ReduceDimension<3>().Normalize();
}

DefaultShader::~DefaultShader()
{
}


void DefaultShader::VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position)
{
}

bool DefaultShader::FragmentShader(const Vec3 barycenter, TGAColor& gl_FragColor)
{
	return false;
}

