#include "Transform.h"

Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up)
{
	// http://www.songho.ca/opengl/gl_camera.html
	Vec<3> z = eye - target;
	Vec<3> targetZAxis = UniformVec<Vec<3>>(z);
	Vec<3> x = Cross(up, targetZAxis);
	Vec<3> targetXAxis = UniformVec<Vec<3>>(x);
	Vec<3> targetYAxis = Cross(targetZAxis, targetXAxis);

	Mat<4, 4> Mat;
	Mat.rows[0] = targetXAxis.AddDimension1(-Dot(targetXAxis, eye));
	Mat.rows[1] = targetYAxis.AddDimension1(-Dot(targetYAxis, eye));
	Mat.rows[2] = targetZAxis.AddDimension1(-Dot(targetZAxis, eye));
	Mat.rows[3] = {0, 0, 0, 1};

	return Mat;
}

Mat<4, 4> Mat4Perspective(float fovy, float aspect, float, float)
{
	return Mat<4, 4>();
}

