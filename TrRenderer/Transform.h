#pragma once
#include "Maths.h"
#include "Shader.h"
#include "TGA.h"
#include <cmath>

// http://www.songho.ca/opengl/gl_projectionmatrix.html#perspective
// baseline：按照gl的规则来实现transform
// truncated pyramid frustum (eye coordinates) 左l，右r，近-n， 远-f，从原点看向z轴负方向(右手坐标系中)
// NDC 各轴[-1, 1]，变换成看向z轴正方向（NDC转换成左手坐标系）
// 
// frustrum实现按照glFrustrum()
// glFrustrum()参数left, right, bottom, top, near, far
// 
// projection plane是near plane
// 
// 需要构造一个GL_PROJECTION matrix
// 
// eye coordinates [GL_PROJECTION matrix] -> clip coordinate [divide w-component] -> NDC
// 

// http://www.songho.ca/opengl/gl_camera.html
Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up);

// aspect宽高比
// fovy yz平面视野开角
// http://www.songho.ca/opengl/gl_projectionmatrix.html
Mat<4, 4> Mat4Perspective(float fovy, float aspect, float near, float far);
Mat<4, 4> Mat4Projection(float f);
Mat<4, 4> Mat4Viewport(int x, int y, int w, int h);


// 使用的全局变量

// view space到clip space

// 视口变换矩阵是将裁剪空间中的坐标转换为屏幕空间中的坐标的矩阵
// 参数x和y表示视口的左下角坐标，w和h表示视口的宽度和高度
// Viewport矩阵的第一列表示x轴的变换，第二列表示y轴的变换，第三列表示z轴的变换，第四列表示平移变换


#ifndef MVP
#define MVP



#endif

Vec3 Barycentric(const Vec2 tri[3], const Vec2 p);

void Triangle(const Vec<4> clipVerts[3], IShader& shader, TGAImage& image, std::vector<float>& zbuffer);