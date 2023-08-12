#include "pch.h"
#include "Camera.h"

Camera::Camera(Vec3 e, Vec3 t, Vec3 u, float asp, float fov)
{
	eye = e;
	target = t;
	up = u;
	aspect = asp;
	fovY = fov;
}

void Camera::CalcAxis()
{
	Vec3 vecz = eye - target;
	k = UniformVec<Vec3>(vecz);
	TrDebug::PrintArray(k, false, "CalcAxis k");
	Vec3 vecx = Cross(vecz, up);
	i = UniformVec<Vec3>(vecx);
	TrDebug::PrintArray(i, false, "CalcAxis i");
	Vec3 vecy = Cross(vecz, vecx);
	j = UniformVec<Vec3>(vecy);
	TrDebug::PrintArray(j, false, "CalcAxis j");
}

// http://www.songho.ca/opengl/gl_camera.html
// basis change
// ix jx kx 0
// iy jy ky 0
// iz jz kz 0
// 0  0  0  1
// scene rotate reversely
// ix iy iz 0
// jx jy jz 0
// kx ky kz 0
// 0  0  0  1
// for rotate matrix, reverse mat == transpose mat
void Camera::SetMatLookAt()
{
	CalcAxis();
	
	Mat<4, 4> mat; // view matrix's rotation part consists of i row, j row, k row
	mat.rows[0] = i.AddDimension1(-Dot(i, eye));
	mat.rows[1] = j.AddDimension1(-Dot(j, eye));
	mat.rows[2] = k.AddDimension1(-Dot(k, eye));
	float tmp[] = { 0, 0, 0, 1 };
	mat.rows[3] = tmp;

	matLookAt = mat;
}

Mat<4, 4> Camera::GetMatLookAt() const
{
	return matLookAt;
}
