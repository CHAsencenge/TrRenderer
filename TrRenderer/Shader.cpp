#include "pch.h"
#include "Shader.h"

extern Mat<4, 4> ModelView;
extern Mat<4, 4> Projection;


// 用floot函数将float类型的采样点坐标向下取整
TGAColor IShader::Sample2D(TGAImage& img, Vec2 uv)
{
	return img.Get(floor(uv.e[0] * img.Width()), floor(uv.e[1] * img.Height()));
}


DefaultShader::DefaultShader(const Model& m, Vec3& lightDir) : model(m)
{
	uniformLightDir = (ModelView * lightDir.AddDimension1(0.0f)).ReduceDimension<3>().Normalize();
}



void DefaultShader::VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position)
{
	varUV.SetCol(nthVert, model.UV(ithFace, nthVert));
	std::vector<int> faceVertsIdx = model.FaceVert(ithFace);
	Vec3 vert = model.Vert(faceVertsIdx[nthVert]);
	VertexShader(vert, gl_Position);
}

void DefaultShader::VertexShader(Vec3 vert, Vec<4>& gl_Position)
{
	Vec<4> vertHomo = vert.AddDimension1(1);
	Vec<4> vertEyeSpace = ModelView * vertHomo;
	Vec<4> vertClipSpace = Projection * vertEyeSpace;
	gl_Position = vertClipSpace;
}

bool DefaultShader::FragmentShader(const Vec3 barycenter, TGAColor& gl_FragColor)
{
	// 法线插值
	// UV插值
	// 
	// 返回值确定此片元是否被丢弃
	return false;
}

