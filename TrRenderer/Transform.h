#pragma once
#include "Maths.h"

// http://www.songho.ca/opengl/gl_camera.html
Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up);

// aspect宽高比
// fovy yz平面视野开角
Mat<4, 4> Mat4Perspective(float fovy, float aspect, float near, float far);