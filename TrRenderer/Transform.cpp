#include "pch.h"
#include "Transform.h"

// 相机固定在(0, 0, 0)，看向-z轴方向，反向旋转场景
// M_modelview = M_view * M_model
// 场景中的每个物体用它自己的M_model进行transform，然后整个场景用M_view反向transform
// Camera的lookAt变换包括两步，整个场景从eye position反向translate到origin（M_T）,将整个场景反向旋转（M_R）
// (见网址中的gif图)
// 计算M_R，先计算forward vector f，然后和up进行cross计算出left，然后f和leftcross进行cross重算出up
// invert M_R，因为是反向旋转场景物体（假如相机在物体上方，物体要向下转，见网址gif图）
// 

// M_T
// 1 0 0 -xe
// 0 1 0 -ye
// 0 0 1 -ze
// 0 0 0   1
// 
// lx ux fx 0
// ly uy fy 0
// lz uz fz 0
//  0  0  0 1
// M_R
// lx ly lz 0
// ux uy uz 0
// fx fy fz 0
//  0  0  0 1
// 
// M_view = M_R * M_T (以下函数的mat)


Mat<4, 4> ModelView;
Mat<4, 4> Projection;
Mat<4, 4> Viewport;

// pending discard
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
	float tmp[] = { 0, 0, 0, 1 };
	mat.rows[3] = tmp;

	return mat;
}


// in perspective projection: truncated pyramid frustum(棱锥台) -> cube(NDC)
// x: [l, r] -> [-1, 1]
// y: [b, t] -> [-1, 1]
// z: [-n, -f] -> [-1, 1]
// 构造GL_PROJECTION matrix
// fovy yz面上的开角（不是半角，所以计算三角函数时需要先除以2）
// aspect 宽高比
Mat<4, 4> Mat4Perspective(float fovY, float aspect, float near, float far)
{
	Mat<4, 4> mat;
	float top = near * tan(fovY / 2);
	float right = aspect * top;
	mat.rows[0] = { near/right, 0, 0, 0 };
	mat.rows[1] = { 0, near/top, 0, 0 };
	mat.rows[2] = { 0, 0, -(far+near)/(far-near), -2*far*near/(far-near) };
	mat.rows[3] = { 0, 0, -1, 0 };
	return mat;
}


// https://github.com/ssloy/tinyrenderer/wiki/Lesson-5-Moving-the-camera
// 这两种投影矩阵的实现区别是什么？
// 相机在z轴c点处，朝向z轴负方向
// 点p在(x, y, z)
// 投影面在z = 0
// 根据相似三角形 x/(c-z) = x'/c  x'是投影面上p的x轴坐标
// 
// 1 0 0 0
// 0 1 0 0
// 0 0 1 0
// 0 0 -1/c 1
// 这个矩阵乘以[x y z 1]^T会得到[x y z 1-(z/c)]
// 再降维则得到[x/(1-(z/c)) y/(1-(z/c)) z/(1-(z/c))]
//
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
	Mat<4, 4> mat;

	return mat;
}

// 见06_解读_
// 包围盒内逐个点判断重心坐标是否满足在三角形内，如在三角形内则填充颜色，否则不填充
// 输入：三个（构成一个三角形的）屏幕空间的二维点，和一个二维点p
// 输出：点p相对于ABC三个点的插值坐标
Vec3 Barycentric(const Vec2 tri[3], const Vec2 p)
{
	Vec3 res;
	// P = A + uAB + vAC
	// 是求u，v的过程
	// uAB(x) + vAC(x) + PA(x) = 0
	// uAB(y) + vAC(y) + PA(y) = 0
	// 找正交于两向量的向量[u v 1]，在计算中仅需两向量求cross
	//
	Vec3 v1 = { tri[2].e[0] - tri[0].e[0], tri[1].e[0] - tri[0].e[0], tri[0].e[0] - p.e[0] };
	Vec3 v2 = { tri[2].e[1] - tri[0].e[1], tri[1].e[1] - tri[0].e[1], tri[0].e[1] - p.e[1] };
	Vec3 orthoVec = Cross(v1, v2);
	
	/*if (std::abs(orthoVec.e[2]) < 1)
		return Vec3(-1.0f, 1.0f, 1.0f);*/
	
	// P = (1-u-v)A + uB + vC
	res = Vec3(1.f-(orthoVec.e[0]+orthoVec.e[1])/orthoVec.e[2], orthoVec.e[1]/ orthoVec.e[2], orthoVec.e[0] / orthoVec.e[2]);
	// TrDebug::PrintArray(res.e, false, "Barycentric");
	return Vec3(1.f-(orthoVec.e[0]+orthoVec.e[1])/orthoVec.e[2], orthoVec.e[1]/ orthoVec.e[2], orthoVec.e[0] / orthoVec.e[2]);
}

// 光栅化方案：包围盒内逐个点依据重心坐标判断是否在三角形内，在三角形内则填充颜色
void Triangle(const Vec<4> clipVerts[3], IShader& shader, TGAImage& image, std::vector<float>& zbuffer)
{
	
	// 透视除法
	// NDC
	Vec3 ndcVerts[3];
	for (int i = 0; i < 3; i++)
	{
		// TrDebug::PrintArray(clipVerts[i].e, false, "Triangle clipVerts: ");
		
		ndcVerts[i].e[0] = clipVerts[i].e[0] / clipVerts[i].e[3];
		ndcVerts[i].e[1] = clipVerts[i].e[1] / clipVerts[i].e[3];
		ndcVerts[i].e[2] = clipVerts[i].e[2] / clipVerts[i].e[3];
		// TrDebug::PrintArray(ndcVerts[i].e, false, "Triangle ndcVerts: ");
	}
	
	// 对顶点做viewport变换
	Vec3 screenVerts[3];
	for (int i = 0; i < 3; i++)
	{
		screenVerts[i].e[0] = ndcVerts[i].e[0] * (image.Width() / 2.0f) + (image.Width() / 2.0f);
		screenVerts[i].e[1] = ndcVerts[i].e[1] * (image.Height() / 2.0f) + (image.Height() / 2.0f);
		screenVerts[i].e[2] = -clipVerts[i].e[2]; // 添加负号的原因，见Transform.h中的统一标准

		// TrDebug::PrintArray(screenVerts[i].e, false, "Triangle screenVerts: ");
	}
	
	// 计算光栅化范围，考虑image范围限制
	float boundingBoxMinX = (std::numeric_limits<float>::max)();
	float boundingBoxMinY = (std::numeric_limits<float>::max)();
	float boundingBoxMaxX = (std::numeric_limits<float>::min)();
	float boundingBoxMaxY = (std::numeric_limits<float>::min)();
	
	// 根据三个顶点确定包围盒范围
	for (int i = 0; i < 3; i++)
	{
		boundingBoxMinX = std::max(0.0f, std::min(boundingBoxMinX, screenVerts[i].e[0]));
		boundingBoxMinY = std::max(0.0f, std::min(boundingBoxMinY, screenVerts[i].e[1]));
		boundingBoxMaxX = std::min(image.Width() - 1.0f, std::max(boundingBoxMaxX, screenVerts[i].e[0]));
		boundingBoxMaxY = std::min(image.Height() - 1.0f, std::max(boundingBoxMaxY, screenVerts[i].e[1]));
		std::cout << "Triangle boundingBoxMinX: " << boundingBoxMinX << std::endl;
		std::cout << "Triangle boundingBoxMinY: " << boundingBoxMinY << std::endl;
		std::cout << "Triangle boundingBoxMaxX: " << boundingBoxMaxX << std::endl;
		std::cout << "Triangle boundingBoxMaxY: " << boundingBoxMaxY << std::endl;
	}
	
	// 遍历光栅化范围xy
	Vec2 screenVerts2D[] = { screenVerts[0].ReduceDimension<2>(), screenVerts[1].ReduceDimension<2>(), screenVerts[2].ReduceDimension<2>() };
	for (int i = static_cast<int>(boundingBoxMinX); i <= static_cast<int>(boundingBoxMaxX); i++)
	{
		for (int j = static_cast<int>(boundingBoxMinY); j <= static_cast<int>(boundingBoxMaxY); j++)
		{
			// std::cout << "Triangle pixel " << i << " " << j << std::endl;
			Vec2 p = { static_cast<float>(i), static_cast<float>(j) };
			Vec3 bcScreen = Barycentric(screenVerts2D, p);
			if (bcScreen.e[0] < 0 || bcScreen.e[1] < 0 || bcScreen.e[2] < 0)
				continue;
			// fragment shader中计算颜色
			TGAColor color;
			// std::cout << "Triangle pixel FragmentShader " << i << " " << j << std::endl;
			bool bDiscard = shader.FragmentShader(bcScreen, color);
			image.Set(i, j, color);
		}
	}
	// 
	//
}

