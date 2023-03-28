#pragma once
#include <cMath>
#include <iostream>

class Vec2
{
public:
	Vec2();
	Vec2(float v0, float v1);
	~Vec2();

	float x() const;
	float y() const;
	float& operator[](int i);
	const float& operator[](int i) const;

	Vec2 operator-() const;
	Vec2& operator+=(const Vec2& x);
	Vec2& operator*=(const float t);
	Vec2& operator/=(const float t);

	float Norm() const;
	float NormSquared() const;

	float e[2];
};

class Vec3
{
public:
	Vec3();
	Vec3(float v0, float v1, float v2);
	~Vec3();

	float x() const;
	float y() const;
	float z() const;
	float& operator[](int i);
	const float& operator[](int i) const;
	Vec3& operator=(Vec3 v1);

	Vec3 operator-() const;
	Vec3& operator+=(const Vec3& x);
	Vec3& operator*=(const float t);
	Vec3& operator/=(const float t);

	float Norm() const;
	float NormSquared() const;
	class Vec4 AddDimension1(float n);

	float e[3];
};

class Vec4 
{
public:
	Vec4();
	Vec4(float e0, float e1, float e2, float e3);

	float x() const;
	float y() const;
	float z() const;
	float w() const;
	float& operator[](int i);
	const float& operator[](int i) const;
	Vec4& operator=(Vec4 v1);

	Vec4& operator*=(const float t);
	Vec4& operator/=(const float t);

public:
	float e[4];
};

class Mat3
{
public:
	Mat3();
	Mat3(Vec3 row0, Vec3 row1, Vec3 row2);
	~Mat3();

	Vec3& operator[](int i);
	const Vec3& operator[](int i) const;

	Mat3 Transpose();
	Mat3 Inverse();
	Mat3 InverseTranspose();
	static Mat3 Identity();
	float Minor(int row, int col) const;
	float Cofactor(int row, int col) const;
	float Det() const;

	Vec3 rows[3];
};

class Mat4
{
public:
	Mat4();
	Mat4(Vec4 row0, Vec4 row1, Vec4 row2, Vec4 row3);
	~Mat4();

	Vec4& operator[](int i);
	const Vec4& operator[](int i) const;

	Mat4 Transpose() const;
	Mat4 Inverse() const;
	Mat4 InverseTranspose() const;
	// 余子式
	float Minor(int row, int col) const;
	// 代数余子式
	float Cofactor(int row, int col) const;
	// 行列式
	float Det() const;
	static Mat4 Identity();

	Vec4 rows[4];
};


// 双目，不定义为类成员函数

// Vec3
Vec3 operator+(const Vec3& m, const Vec3& n);
Vec3 operator-(const Vec3& m, const Vec3& n);
Vec3 operator*(const Vec3& m, const Vec3& n);
Vec3 operator*(double t, const Vec3& n);
Vec3 operator*(const Vec3& n, double t);
Vec3 operator/(Vec3 n, double t);


namespace MathUtil
{
	double Dot(const Vec3& m, const Vec3& n);
	Vec3 Cross(const Vec3& m, const Vec3& n);

	template<typename T>
	T UniformVec(T& Vec);

}

template<int nrow, int ncol> 
class Mat
{
public:
	Vec4& operator[](int i);
	const Vec4& operator[](int i) const;

	Mat4 Transpose() const;
	Mat4 Inverse() const;
	Mat4 InverseTranspose() const;
	// 余子式
	float Minor(int row, int col) const;
	// 代数余子式
	float Cofactor(int row, int col) const;
	// 行列式
	float Det() const;
	static Mat4 Identity();

	Vec<ncol> rows[nrow];
};

template<int nelement>
class Vec
{
public:
	float& operator[](int i);
	const float& operator[](int i) const;

	float Norm() const;
	float Norm2() const;
	Vec<nelement> Normalize() const;

	float e[nelement] = {0};
};

template<int nelement>
inline float& Vec<nelement>::operator[](int i)
{
	return e[i];
}

template<int nelement>
inline const float& Vec<nelement>::operator[](int i) const
{
	return e[i];
}

template<int nelement>
inline float Vec<nelement>::Norm() const
{
	return sqrt(Norm2());
}

template<int nelement>
inline float Vec<nelement>::Norm2() const
{
	return *this * *this;
}

template<int nelement>
inline Vec<nelement> Vec<nelement>::Normalize() const
{
	return *this / Norm();
}

template<int nelement>
float operator*(const Vec<nelement>& lhs, const Vec<nelement>& rhs)
{
	float ret = 0;
	for (int i = 0; i < nelement; i++)
	{
		ret = lhs[i] * rhs[i];
	}
	return ret;
}

template<int nelement>
Vec<nelement> operator*(const Vec<nelement>& lhs, float m)
{
	Vec<nelement> vec;
	for (int i = 0; i < nelement; i++)
	{
		vec.e[i] = lhs.e[i] * m;
	}
	return vec;
}

template<int nelement>
Vec<nelement> operator*(float m, const Vec<nelement>& rhs)
{
	return rhs * m;
}

template<int nelement>
Vec<nelement> operator+(const Vec<nelement>& lhs, const Vec<nelement>& rhs)
{
	Vec<nelement> vec = {0};
	for (int i = 0; i < nelement; i++)
	{
		vec.e[i] = lhs[i] + rhs[i];
	}
	return vec;
}

template<int nelement>
Vec<nelement> operator-(const Vec<nelement>& lhs, const Vec<nelement>& rhs)
{
	Vec<nelement> vec = { 0 };
	for (int i = 0; i < nelement; i++)
	{
		vec.e[i] = lhs[i] - rhs[i];
	}
	return vec;
}

template<int nelement>
Vec<nelement> operator/(const Vec<nelement>& lhs, float m)
{
	Vec<nelement> vec;
	for (int i = 0; i < nelement; i++)
	{
		vec.e[i] = lhs.e[i] * m;
	}
	return vec;
}

