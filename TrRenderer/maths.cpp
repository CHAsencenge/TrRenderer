#include "Maths.h"

vec2::vec2() : e{0, 0}
{
}

vec2::vec2(float v0, float v1)
{
	e[0] = v0;
	e[1] = v1;
}

vec2::~vec2()
{
}

float vec2::x() const
{
	return e[0];
}

float vec2::y() const
{
	return e[1];
}

float& vec2::operator[](int i)
{
	return e[i];
}

const float& vec2::operator[](int i) const
{
	return e[i];
}

vec2 vec2::operator-() const
{
	return vec2(-e[0], -e[1]);
}

vec2& vec2::operator+=(const vec2& x)
{
	e[0] += x[0];
	e[1] += x[1];
	return *this;
}

vec2& vec2::operator*=(const float t)
{
	e[0] *= t;
	e[1] *= t;
	return *this;
}

vec2& vec2::operator/=(const float t)
{
	e[0] /= t;
	e[1] /= t;
	return *this;
}

float vec2::norm() const
{
	return sqrt(e[0] * e[0] + e[1] * e[1]);
}

float vec2::norm_squared() const
{
	return e[0] * e[0] + e[1] * e[1];
}


vec3::vec3() : e{ 0, 0, 0 }
{
}

vec3::vec3(float v0, float v1, float v2)
{
	e[0] = v0;
	e[1] = v1;
	e[2] = v2;
}

vec3::~vec3()
{
}

float vec3::x() const
{
	return e[0];
}

float vec3::y() const
{
	return e[1];
}

float vec3::z() const
{
	return e[2];
}

float& vec3::operator[](int i)
{
	return e[i];
}

const float& vec3::operator[](int i) const
{
	return e[i];
}

vec3& vec3::operator=(vec3 v1)
{
	e[0] = v1[0];
	e[1] = v1[1];
	e[2] = v1[2];
	return *this;
}

vec3 vec3::operator-() const
{
	return vec3(-e[0], -e[1], -e[2]);
}

vec3& vec3::operator+=(const vec3& x)
{
	e[0] += x[0];
	e[1] += x[1];
	e[2] += x[2];
	return *this;
}

vec3& vec3::operator*=(const float t)
{
	e[0] *= t;
	e[1] *= t;
	e[2] *= t;
	return *this;
}

vec3& vec3::operator/=(const float t)
{
	e[0] /= t;
	e[1] /= t;
	e[2] /= t;
	return *this;
}

float vec3::norm() const
{
	return sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
}

float vec3::norm_squared() const
{
	return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
}

vec4 vec3::add_dimension_1(float n)
{
	vec4 vec;
	vec[0] = e[0];
	vec[1] = e[1];
	vec[2] = e[2];
	vec[3] = n;
	return vec;
}

/* vec4 class member functions */
vec4::vec4() : e{ 0,0,0,0 } {}
vec4::vec4(float e0, float e1, float e2, float e3) : e{ e0,e1,e2,e3 } {}
float vec4::x() const { return e[0]; }
float vec4::y() const { return e[1]; }
float vec4::z() const { return e[2]; }
float vec4::w() const { return e[3]; }
const float& vec4::operator[](int i) const { return e[i]; }
vec4& vec4::operator=(vec4 v1)
{
	e[0] = v1[0];
	e[1] = v1[1];
	e[2] = v1[2];
	e[3] = v1[3];
	return *this;
}
float& vec4::operator[](int i) { return e[i]; }
vec4& vec4::operator*=(const float t) { e[0] *= t; e[1] *= t; e[2] *= t; e[3] *= t; return *this; }
vec4& vec4::operator/=(const float t) { return *this *= 1 / t; }

/* vec4 related functions */

vec4 to_vec4(const vec3& u, float w)
{
	return vec4(u[0], u[1], u[2], w);
}

std::ostream& operator<<(std::ostream& out, const vec4& e)
{
	return out << e[0] << " " << e[1] << " " << e[2] << " " << e[3];
}

vec4 operator-(const vec4& u, const vec4& v)
{
	return vec4(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2], u.e[3] - v.e[3]);
}

vec4 operator+(const vec4& u, const vec4& v)
{
	return vec4(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2], u.e[3] + v.e[3]);
}

vec4 operator*(double t, const vec4& v)
{
	return vec4(t * v.e[0], t * v.e[1], t * v.e[2], t * v.e[3]);
}

vec4 operator*(const vec4& v, double t)
{
	return t * v;
}

mat3::mat3()
{
}

mat3::mat3(vec3 row0, vec3 row1, vec3 row2)
{
	rows[0] = { 1, 0, 0 };
	rows[1] = { 0, 1, 0 };
	rows[2] = { 0, 0, 1 };
}

mat3::~mat3()
{
}

vec3& mat3::operator[](int i)
{
	return rows[i];
}

const vec3& mat3::operator[](int i) const
{
	return rows[i];
}

mat3 mat3::transpose()
{
	return mat3();
}

mat3 mat3::inverse()
{
	return mat3();
}

mat3 mat3::inverse_transpose()
{
	return mat3();
}

mat3 mat3::identity()
{
	return mat3();
}

mat4::mat4()
{
	rows[0] = { 1, 0, 0, 0 };
	rows[1] = { 0, 1, 0, 0 };
	rows[2] = { 0, 0, 1, 0 };
	rows[3] = { 0, 0, 0, 1 };
}

mat4::mat4(vec4 row0, vec4 row1, vec4 row2, vec4 row3)
{
	rows[0] = row0;
	rows[1] = row1;
	rows[2] = row2;
	rows[3] = row3;
}

mat4::~mat4()
{
}

vec4& mat4::operator[](int i)
{
	return rows[i];
}

const vec4& mat4::operator[](int i) const
{
	return rows[i];
}

mat4 mat4::transpose() const
{
	mat4 mat;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			mat[i][j] = rows[j][i];
		}
	}
	return mat;
}

mat4 mat4::inverse() const
{
	return mat4();
}

mat4 mat4::inverse_transpose() const
{
	return mat4();
}

mat4 mat4::identity()
{
	mat4 mat;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			mat[i][j] = (i == j);
	return mat;
}


vec3 operator+(const vec3& m, const vec3& n)
{
	return vec3(m.e[0] + n.e[0], m.e[1] + n.e[1], m.e[2] + n.e[2]);
}

vec3 operator-(const vec3& m, const vec3& n)
{
	return vec3(m.e[0] - n.e[0], m.e[1] - n.e[1], m.e[2] - n.e[2]);
}

vec3 operator*(const vec3& m, const vec3& n)
{
	return vec3(m.e[0] * n.e[0], m.e[1] * n.e[1], m.e[2] * n.e[2]);
}

vec3 operator*(double t, const vec3& n)
{
	return vec3(t * n.e[0], t * n.e[1], t * n.e[2]);
}

vec3 operator*(const vec3& n, double t)
{
	return t * n;
}

vec3 operator/(vec3 n, double t)
{
	return (1 / t) * n;
}

double dot(const vec3& m, const vec3& n)
{
	return m.e[0] * n.e[0]
		+ m.e[1] * n.e[1]
		+ m.e[2] * n.e[2];
}

vec3 cross(const vec3& m, const vec3& n)
{
	return vec3(m.e[1] * n.e[2] - m.e[2] * n.e[1],
		m.e[2] * n.e[0] - m.e[0] * n.e[2],
		m.e[0] * n.e[1] - m.e[1] * n.e[0]);
}

template<typename T>
T uniform_vec(T& vec)
{
	return T / T.norm();
}