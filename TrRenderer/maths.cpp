#include "maths.h"

vec2::vec2() : v{0, 0}
{
}

vec2::vec2(float v0, float v1)
{
	v[0] = v0;
	v[1] = v1;
}

vec2::~vec2()
{
}

float vec2::x() const
{
	return v[0];
}

float vec2::y() const
{
	return v[1];
}

float& vec2::operator[](int i)
{
	return v[i];
}

const float& vec2::operator[](int i) const
{
	return v[i];
}

vec2 vec2::operator-() const
{
	return vec2(-v[0], -v[1]);
}

vec2& vec2::operator+=(const vec2& x)
{
	v[0] += x[0];
	v[1] += x[1];
	return *this;
}

vec2& vec2::operator*=(const float t)
{
	v[0] *= t;
	v[1] *= t;
	return *this;
}

vec2& vec2::operator/=(const float t)
{
	v[0] /= t;
	v[1] /= t;
	return *this;
}

float vec2::norm() const
{
	return sqrt(v[0] * v[0] + v[1] * v[1]);
}

float vec2::norm_squared() const
{
	return v[0] * v[0] + v[1] * v[1];
}


vec3::vec3() : v{ 0, 0, 0 }
{
}

vec3::vec3(float v0, float v1, float v2)
{
	v[0] = v0;
	v[1] = v1;
	v[2] = v2;
}

vec3::~vec3()
{
}

float vec3::x() const
{
	return v[0];
}

float vec3::y() const
{
	return v[1];
}

float vec3::z() const
{
	return v[2];
}

float& vec3::operator[](int i)
{
	return v[i];
}

const float& vec3::operator[](int i) const
{
	return v[i];
}

vec3 vec3::operator-() const
{
	return vec3(-v[0], -v[1], -v[2]);
}

vec3& vec3::operator+=(const vec3& x)
{
	v[0] += x[0];
	v[1] += x[1];
	v[2] += x[2];
	return *this;
}

vec3& vec3::operator*=(const float t)
{
	v[0] *= t;
	v[1] *= t;
	v[2] *= t;
	return *this;
}

vec3& vec3::operator/=(const float t)
{
	v[0] /= t;
	v[1] /= t;
	v[2] /= t;
	return *this;
}

float vec3::norm() const
{
	return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

float vec3::norm_squared() const
{
	return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}


/* vec4 class member functions */
vec4::vec4() : e{ 0,0,0,0 } {}
vec4::vec4(float e0, float e1, float e2, float e3) : e{ e0,e1,e2,e3 } {}
float vec4::x() const { return e[0]; }
float vec4::y() const { return e[1]; }
float vec4::z() const { return e[2]; }
float vec4::w() const { return e[3]; }
const float& vec4::operator[](int i) const { return e[i]; }
float& vec4::operator[](int i) { return e[i]; }
vec4& vec4::operator*=(const float t) { e[0] *= t; e[1] *= t; e[2] *= t; e[3] *= t; return *this; }
vec4& vec4::operator/=(const float t) { return *this *= 1 / t; }

/* vec4 related functions */

vec4 to_vec4(const vec3& u, float w)
{
	return vec4(u[0], u[1], u[2], w);
}

std::ostream& operator<<(std::ostream& out, const vec4& v)
{
	return out << v[0] << " " << v[1] << " " << v[2] << " " << v[3];
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
