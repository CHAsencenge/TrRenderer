#include "Maths.h"

float Dot(const Vec3& m, const Vec3& n)
{
	float ret = 0;
	for (int i = 0; i < 3; i++)
	{
		ret += m.e[i] * n.e[i];
	}
	return ret;
}

Vec3 Cross(const Vec3& m, const Vec3& n)
{
	Vec3 vec;
	vec.e[0] = m.e[1] * n.e[2] - m.e[2] * n.e[1];
	vec.e[1] = m.e[2] * n.e[0] - m.e[0] * n.e[2];
	vec.e[2] = m.e[0] * n.e[1] - m.e[1] * n.e[0];
	return vec;
}
