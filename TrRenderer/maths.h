#pragma once
#include <cmath>
#include <iostream>

class vec2
{
public:
	vec2();
	vec2(float v0, float v1);
	~vec2();

	float x() const;
	float y() const;
	float& operator[](int i);
	const float& operator[](int i) const;

	vec2 operator-() const;
	vec2& operator+=(const vec2& x);
	vec2& operator*=(const float t);
	vec2& operator/=(const float t);

	float norm() const;
	float norm_squared() const;

	float e[2];
};

class vec3
{
public:
	vec3();
	vec3(float v0, float v1, float v2);
	~vec3();

	float x() const;
	float y() const;
	float z() const;
	float& operator[](int i);
	const float& operator[](int i) const;
	vec3& operator=(vec3 v1);

	vec3 operator-() const;
	vec3& operator+=(const vec3& x);
	vec3& operator*=(const float t);
	vec3& operator/=(const float t);

	float norm() const;
	float norm_squared() const;
	class vec4 add_dimension_1(float n);

	float e[3];
};

class vec4 
{
public:
	vec4();
	vec4(float e0, float e1, float e2, float e3);

	float x() const;
	float y() const;
	float z() const;
	float w() const;
	float& operator[](int i);
	const float& operator[](int i) const;
	vec4& operator=(vec4 v1);

	vec4& operator*=(const float t);
	vec4& operator/=(const float t);

public:
	float e[4];
};

class mat3
{
public:
	mat3();
	mat3(vec3 row0, vec3 row1, vec3 row2);
	~mat3();

	vec3& operator[](int i);
	const vec3& operator[](int i) const;

	mat3 transpose();
	mat3 inverse();
	mat3 inverse_transpose();
	static mat3 identity();

	vec3 rows[3];
};

class mat4
{
public:
	mat4();
	mat4(vec4 row0, vec4 row1, vec4 row2, vec4 row3);
	~mat4();

	vec4& operator[](int i);
	const vec4& operator[](int i) const;

	mat4 transpose() const;
	mat4 inverse() const;
	mat4 inverse_transpose() const;
	static mat4 identity();

	vec4 rows[4];
};


// 双目，不定义为类成员函数

// vec3
vec3 operator+(const vec3& m, const vec3& n);
vec3 operator-(const vec3& m, const vec3& n);
vec3 operator*(const vec3& m, const vec3& n);
vec3 operator*(double t, const vec3& n);
vec3 operator*(const vec3& n, double t);
vec3 operator/(vec3 n, double t);
double dot(const vec3& m, const vec3& n);
vec3 cross(const vec3& m, const vec3& n);


template<typename T>
T uniform_vec(T& vec);

