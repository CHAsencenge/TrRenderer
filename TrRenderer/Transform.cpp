#include "Transform.h"

Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up)
{
	// http://www.songho.ca/opengl/gl_camera.html
	Vec<3> z = eye - target;
	Vec<3> targetZAxis = UniformVec<Vec<3>>(z);
	Vec<3> x = Cross(up, targetZAxis);
	Vec<3> targetXAxis = UniformVec<Vec<3>>(x);
	Vec<3> targetYAxis = Cross(targetZAxis, targetXAxis);

	Mat<4, 4> mat;
	mat.rows[0] = targetXAxis.AddDimension1(-Dot(targetXAxis, eye));
	mat.rows[1] = targetYAxis.AddDimension1(-Dot(targetYAxis, eye));
	mat.rows[2] = targetZAxis.AddDimension1(-Dot(targetZAxis, eye));
	mat.rows[3] = {0, 0, 0, 1};

	return mat;
}

// aspect宽高比
// fovy yz平面视野开角
Mat<4, 4> Mat4Perspective(float fovy, float aspect, float near, float far)
{
	Mat<4, 4> mat;
	float top = near * tan(fovy / 2);
	float right = aspect * top;
	mat.rows[0] = { near/right, 0, 0, 0 };
	mat.rows[1] = { 0, near/top, 0, 0 };
	mat.rows[2] = { 0, 0, -(far+near)/(far-near), -2*far*near/(far-near) };
	mat.rows[3] = { 0, 0, -1, 0 };
	return mat;
}

