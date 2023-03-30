#pragma once
#include "Maths.h"

class Camera
{
public:
	Camera(Vec3 e, Vec3 t, Vec3 u, float asp, float fov);
	~Camera();

	// 根据朝向计算此相机的坐标轴
	void CalcAxis();

private:
	float fovy;
	float aspect;
	Vec3 eye;
	Vec3 target;
	Vec3 up;

	Vec3 x;
	Vec3 y;
	Vec3 z;
};