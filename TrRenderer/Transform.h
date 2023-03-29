#pragma once
#include "Maths.h"

Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up);

Mat<4, 4> Mat4Perspective(float fovy, float aspect, float near, float far);