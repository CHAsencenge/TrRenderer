#include "Transform.h"

Mat4 Transform::Mat4Lookat(Vec3 eye, Vec3 target, Vec3 up)
{
	// http://www.songho.ca/opengl/gl_camera.html
	Vec3 z = eye - target;
	Vec3 targetZAxis = MathUtil::UniformVec<Vec3>(z);
	Vec3 x = MathUtil::Cross(up, targetZAxis);
	Vec3 targetXAxis = MathUtil::UniformVec<Vec3>(x);
	Vec3 targetYAxis = MathUtil::Cross(targetZAxis, targetXAxis);

	Mat4 Mat;
	Mat.rows[0] = targetXAxis.AddDimension1(-MathUtil::Dot(targetXAxis, eye));
	Mat.rows[1] = targetYAxis.AddDimension1(-MathUtil::Dot(targetYAxis, eye));
	Mat.rows[2] = targetZAxis.AddDimension1(-MathUtil::Dot(targetZAxis, eye));
	Mat.rows[3] = {0, 0, 0, 1};

	return Mat;
}

