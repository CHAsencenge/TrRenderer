#include "Transform.h"

// https://github.com/ssloy/tinyrenderer/wiki/Lesson-5-Moving-the-camera
// 相机固定在(0, 0, 0)，看向-z轴方向，反向旋转场景
// M_modelview = M_view * M_model
// 场景中的每个物体用它自己的M_model进行transform，然后整个场景用M_view反向transform
// Camera的lookAt变换包括两步，整个场景从eye position反向translate到origin（M_T）,将整个场景反向旋转（M_R）
// (见网址中的gif图)
// 计算M_R，先计算forward vector f，然后和up进行cross计算出left，然后f和leftcross进行cross重算出up
// invert M_R，因为是反向旋转场景物体（假如相机在物体上方，物体要向下转，见网址gif图）
// 
Mat<4, 4> Mat4Lookat(Vec<3> eye, Vec<3> target, Vec<3> up)
{
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


// 这两种投影矩阵的实现区别是什么？
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

// f表示视景体的远平面距离，它用于将z轴上的坐标从[-f, f]的范围映射到[-1, 1]的范围
Mat<4, 4> Mat4Projection(float f)
{
	Mat<4, 4> mat;
	mat.rows[0] = { 1, 0, 0, 0 };
	mat.rows[1] = { 0,-1, 0, 0 };
	mat.rows[2] = { 0, 0, 1, 0 };
	mat.rows[3] = { 0, 0,-1/f,0 };
	return mat;
}

Mat<4, 4> Mat4Viewport(int x, int y, int w, int h)
{
	return Mat<4, 4>();
}

