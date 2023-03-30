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
