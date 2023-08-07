#pragma once
#include "Model.h"
#include "Maths.h"
#include <cmath>

typedef struct Cubemap
{
	TGAImage* faces[6];
}Cubemap_t;

// 基类写成接口类，提供方法供各种子类实现
class IShader
{
public:
	IShader() {}
	~IShader() {}

	// 供fragment shader使用
	static TGAColor Sample2D(TGAImage& img, Vec2 uv);
	// model view projection
	// 输出clip space的顶点到gl_Position
	virtual void VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position) = 0;
	// 直接传顶点作为参数的vertex shader方法
	virtual void VertexShader(Vec3 vert, Vec<4>& gl_Position) = 0;
	// 返回该pixel是否被discard
	virtual bool FragmentShader(const Vec3 barycenter, TGAColor& gl_FragColor) = 0;

private:
};

class DefaultShader : public IShader
{
public:
	DefaultShader(const Model& m, Vec3& lightDir);
	~DefaultShader() {}

	virtual void VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position) override;
	virtual void VertexShader(Vec3 vert, Vec<4>& gl_Position) override;
	virtual bool FragmentShader(const Vec3 barycenter, TGAColor& gl_FragColor) override;

private:
	Model model;
	// view coordinates下的信息
	Vec3 uniformLightDir;
	// 为什么是Mat的，以三角形为单位来三个三个执行Vertex Shader，存储写入的信息供给Frag Shader
	Mat<3, 3> matVertexVerts;
	Mat<2, 3> matVertexUVs;
	Mat<3, 3> matVertexNormals;
	// view coordinate下的三角形
	Mat<3, 3> viewCoordTriangle;

};