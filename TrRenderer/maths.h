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

	float v[2];
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

	vec3 operator-() const;
	vec3& operator+=(const vec3& x);
	vec3& operator*=(const float t);
	vec3& operator/=(const float t);

	float norm() const;
	float norm_squared() const;

	float v[3];
};

class vec4 {
public:
	vec4();
	vec4(float e0, float e1, float e2, float e3);

	float x() const;
	float y() const;
	float z() const;
	float w() const;
	float& operator[](int i);
	const float& operator[](int i) const;

	vec4& operator*=(const float t);
	vec4& operator/=(const float t);

public:
	float e[4];
};