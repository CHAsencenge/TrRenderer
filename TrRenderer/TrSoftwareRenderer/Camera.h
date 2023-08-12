#pragma once
// 还是需要include预编译头
#include "pch.h"
#include "Maths.h"

class Camera
{
public:
	Camera() 
	{
		// 带f后缀以单精度处理
		fovY = PI / 3.0f;
		aspect = 4.0f / 3.0f;
		// constexpr编译时确定值，运行时不产生开销，优化
		constexpr float e[] = { 5, 0, 0 };
		constexpr float t[] = { 0, 0, 0 };
		constexpr float u[] = { 0, 1, 0 };
		eye = e;
		target = t;
		up = u;
	}
	Camera(Vec3 e, Vec3 t, Vec3 u, float asp, float fov);
	~Camera() = default;

	// 根据朝向计算此相机的坐标轴
	void CalcAxis();

	void SetMatLookAt();
	Mat<4, 4> GetMatLookAt() const;

public:
	float fovY;
	float aspect;
	Vec3 eye;
	Vec3 target;
	Vec3 up;

private:
	// eye space axis (basis)
	Vec3 i;
	Vec3 j;
	Vec3 k;

	Mat<4, 4> matLookAt;

	// 用x, y, z是否可以直接构造view矩阵
};