#include "Maths.h"




Vec3 operator+(const Vec3& m, const Vec3& n)
{
	return Vec3(m.e[0] + n.e[0], m.e[1] + n.e[1], m.e[2] + n.e[2]);
}

Vec3 operator-(const Vec3& m, const Vec3& n)
{
	return Vec3(m.e[0] - n.e[0], m.e[1] - n.e[1], m.e[2] - n.e[2]);
}

Vec3 operator*(const Vec3& m, const Vec3& n)
{
	return Vec3(m.e[0] * n.e[0], m.e[1] * n.e[1], m.e[2] * n.e[2]);
}

Vec3 operator*(double t, const Vec3& n)
{
	return Vec3(t * n.e[0], t * n.e[1], t * n.e[2]);
}

Vec3 operator*(const Vec3& n, double t)
{
	return t * n;
}

Vec3 operator/(Vec3 n, double t)
{
	return (1 / t) * n;
}

double MathUtil::Dot(const Vec3& m, const Vec3& n)
{
	return m.e[0] * n.e[0]
		+ m.e[1] * n.e[1]
		+ m.e[2] * n.e[2];
}

Vec3 MathUtil::Cross(const Vec3& m, const Vec3& n)
{
	return Vec3(m.e[1] * n.e[2] - m.e[2] * n.e[1],
		m.e[2] * n.e[0] - m.e[0] * n.e[2],
		m.e[0] * n.e[1] - m.e[1] * n.e[0]);
}

template<typename T>
T MathUtil::UniformVec(T& Vec)
{
	return T / T.Norm();
}