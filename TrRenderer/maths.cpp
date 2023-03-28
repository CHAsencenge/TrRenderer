#include "Maths.h"

Vec2::Vec2() : e{0, 0}
{
}

Vec2::Vec2(float v0, float v1)
{
	e[0] = v0;
	e[1] = v1;
}

Vec2::~Vec2()
{
}

float Vec2::x() const
{
	return e[0];
}

float Vec2::y() const
{
	return e[1];
}

float& Vec2::operator[](int i)
{
	return e[i];
}

const float& Vec2::operator[](int i) const
{
	return e[i];
}

Vec2 Vec2::operator-() const
{
	return Vec2(-e[0], -e[1]);
}

Vec2& Vec2::operator+=(const Vec2& x)
{
	e[0] += x[0];
	e[1] += x[1];
	return *this;
}

Vec2& Vec2::operator*=(const float t)
{
	e[0] *= t;
	e[1] *= t;
	return *this;
}

Vec2& Vec2::operator/=(const float t)
{
	e[0] /= t;
	e[1] /= t;
	return *this;
}

float Vec2::Norm() const
{
	return sqrt(e[0] * e[0] + e[1] * e[1]);
}

float Vec2::NormSquared() const
{
	return e[0] * e[0] + e[1] * e[1];
}


Vec3::Vec3() : e{ 0, 0, 0 }
{
}

Vec3::Vec3(float v0, float v1, float v2)
{
	e[0] = v0;
	e[1] = v1;
	e[2] = v2;
}

Vec3::~Vec3()
{
}

float Vec3::x() const
{
	return e[0];
}

float Vec3::y() const
{
	return e[1];
}

float Vec3::z() const
{
	return e[2];
}

float& Vec3::operator[](int i)
{
	return e[i];
}

const float& Vec3::operator[](int i) const
{
	return e[i];
}

Vec3& Vec3::operator=(Vec3 v1)
{
	e[0] = v1[0];
	e[1] = v1[1];
	e[2] = v1[2];
	return *this;
}

Vec3 Vec3::operator-() const
{
	return Vec3(-e[0], -e[1], -e[2]);
}

Vec3& Vec3::operator+=(const Vec3& x)
{
	e[0] += x[0];
	e[1] += x[1];
	e[2] += x[2];
	return *this;
}

Vec3& Vec3::operator*=(const float t)
{
	e[0] *= t;
	e[1] *= t;
	e[2] *= t;
	return *this;
}

Vec3& Vec3::operator/=(const float t)
{
	e[0] /= t;
	e[1] /= t;
	e[2] /= t;
	return *this;
}

float Vec3::Norm() const
{
	return sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
}

float Vec3::NormSquared() const
{
	return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
}

Vec4 Vec3::AddDimension1(float n)
{
	Vec4 Vec;
	Vec[0] = e[0];
	Vec[1] = e[1];
	Vec[2] = e[2];
	Vec[3] = n;
	return Vec;
}

/* Vec4 class member functions */
Vec4::Vec4() : e{ 0,0,0,0 } {}
Vec4::Vec4(float e0, float e1, float e2, float e3) : e{ e0,e1,e2,e3 } {}
float Vec4::x() const { return e[0]; }
float Vec4::y() const { return e[1]; }
float Vec4::z() const { return e[2]; }
float Vec4::w() const { return e[3]; }
const float& Vec4::operator[](int i) const { return e[i]; }
Vec4& Vec4::operator=(Vec4 v1)
{
	e[0] = v1[0];
	e[1] = v1[1];
	e[2] = v1[2];
	e[3] = v1[3];
	return *this;
}
float& Vec4::operator[](int i) { return e[i]; }
Vec4& Vec4::operator*=(const float t) { e[0] *= t; e[1] *= t; e[2] *= t; e[3] *= t; return *this; }
Vec4& Vec4::operator/=(const float t) { return *this *= 1 / t; }

/* Vec4 related functions */

Vec4 to_Vec4(const Vec3& u, float w)
{
	return Vec4(u[0], u[1], u[2], w);
}

std::ostream& operator<<(std::ostream& out, const Vec4& e)
{
	return out << e[0] << " " << e[1] << " " << e[2] << " " << e[3];
}

Vec4 operator-(const Vec4& u, const Vec4& v)
{
	return Vec4(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2], u.e[3] - v.e[3]);
}

Vec4 operator+(const Vec4& u, const Vec4& v)
{
	return Vec4(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2], u.e[3] + v.e[3]);
}

Vec4 operator*(double t, const Vec4& v)
{
	return Vec4(t * v.e[0], t * v.e[1], t * v.e[2], t * v.e[3]);
}

Vec4 operator*(const Vec4& v, double t)
{
	return t * v;
}

Mat3::Mat3()
{
}

Mat3::Mat3(Vec3 row0, Vec3 row1, Vec3 row2)
{
	rows[0] = { 1, 0, 0 };
	rows[1] = { 0, 1, 0 };
	rows[2] = { 0, 0, 1 };
}

Mat3::~Mat3()
{
}

Vec3& Mat3::operator[](int i)
{
	return rows[i];
}

const Vec3& Mat3::operator[](int i) const
{
	return rows[i];
}

Mat3 Mat3::Transpose()
{
	return Mat3();
}

Mat3 Mat3::Inverse()
{
	return Mat3();
}

Mat3 Mat3::InverseTranspose()
{
	return Mat3();
}

Mat3 Mat3::Identity()
{
	return Mat3();
}

float Mat3::Minor(int row, int col) const
{
	return 0.0f;
}

float Mat3::Cofactor(int row, int col) const
{
	return 0.0f;
}

float Mat3::Det() const
{
	return 0.0f;
}

Mat4::Mat4()
{
	rows[0] = { 1, 0, 0, 0 };
	rows[1] = { 0, 1, 0, 0 };
	rows[2] = { 0, 0, 1, 0 };
	rows[3] = { 0, 0, 0, 1 };
}

Mat4::Mat4(Vec4 row0, Vec4 row1, Vec4 row2, Vec4 row3)
{
	rows[0] = row0;
	rows[1] = row1;
	rows[2] = row2;
	rows[3] = row3;
}

Mat4::~Mat4()
{
}

Vec4& Mat4::operator[](int i)
{
	return rows[i];
}

const Vec4& Mat4::operator[](int i) const
{
	return rows[i];
}

Mat4 Mat4::Transpose() const
{
	Mat4 Mat;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			Mat[i][j] = rows[j][i];
		}
	}
	return Mat;
}

Mat4 Mat4::Inverse() const
{
	return Mat4();
}

Mat4 Mat4::InverseTranspose() const
{
	return Mat4();
}

float Mat4::Minor(int row, int col) const
{
	Mat3 mat;
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			int x = i < row ? i : i + 1;
			int y = j < col ? j : j + 1;
			mat.rows[i][j] = rows[x][y];
		}
	}
	return mat.Det();
}

float Mat4::Cofactor(int row, int col) const
{
	return pow(-1, row + col) * Minor(row, col);
}

float Mat4::Det() const
{
	float res = 0;
	for (int i = 0; i < 4; i++)
	{
		res += Cofactor(0, i);
	}
	return res;
}

Mat4 Mat4::Identity()
{
	Mat4 Mat;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			Mat[i][j] = (i == j);
	return Mat;
}


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