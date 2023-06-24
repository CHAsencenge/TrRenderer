#include "pch.h"
#include "Camera.h"

Camera::Camera(Vec3 e, Vec3 t, Vec3 u, float asp, float fov)
{
	eye = e;
	target = t;
	up = u;
	aspect = asp;
	fovy = fov;
}

Camera::~Camera()
{
}

void Camera::CalcAxis()
{
	Vec3 vecz = eye - target;
	z = UniformVec<Vec3>(vecz);
	Vec3 vecx = Cross(vecz, up);
	x = UniformVec<Vec3>(vecx);
	Vec3 vecy = Cross(vecz, vecx);
	y = UniformVec<Vec3>(vecy);
}

void Camera::SetMatLookAt()
{
	CalcAxis();
	
	Mat<4, 4> mat;
	mat.rows[0] = x.AddDimension1(-Dot(x, eye));
	mat.rows[1] = y.AddDimension1(-Dot(y, eye));
	mat.rows[2] = z.AddDimension1(-Dot(z, eye));
	float tmp[] = { 0, 0, 0, 1 };
	mat.rows[3] = tmp;

	matLookAt = mat;
}

Mat<4, 4> Camera::GetMatLookAt() const
{
	return matLookAt;
}
