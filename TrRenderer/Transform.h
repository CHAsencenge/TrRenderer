#pragma once
#include "Maths.h"

// http://www.songho.ca/opengl/gl_camera.html
Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up);

// aspect宽高比
// fovy yz平面视野开角
Mat<4, 4> Mat4Perspective(float fovy, float aspect, float near, float far);
Mat<4, 4> Mat4Projection(float f);
Mat<4, 4> Mat4Viewport(int x, int y, int w, int h);


// 使用的全局变量
Mat<4, 4> ModelView;
// view space到clip space
Mat<4, 4> Projection;
// 视口变换矩阵是将裁剪空间中的坐标转换为屏幕空间中的坐标的矩阵
// 参数x和y表示视口的左下角坐标，w和h表示视口的宽度和高度
// Viewport矩阵的第一列表示x轴的变换，第二列表示y轴的变换，第三列表示z轴的变换，第四列表示平移变换
Mat<4, 4> Viewport;