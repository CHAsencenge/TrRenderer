#pragma once
#include <cmath>
#include <vector>
#include <iostream>
#include <cassert>


template<int nelement>
class Vec
{
public:
	Vec() {};
	// 使得Vec可以用参数列表进行初始化，但仅兼容到最多4个元素的Vec
	// e0不给默认初值，否则无法和默认构造函数区分开
	Vec(float e0, float e1 = 0, float e2 = 0, float e3 = 0)
	{
		int count = 0;
		e[0] = e0;
		count++;
		if (count < nelement)
		{
			e[1] = e1;
			count++;
		}
		if (count < nelement)
		{
			e[2] = e2;
			count++;
		}
		if (count < nelement)
		{
			e[3] = e3;
			count++;
		}
	}
	Vec(const float other[])
	{
		for (int i = 0; i < nelement; i++)
		{
			e[i] = other[i];
		}
	}
	Vec(const Vec<nelement>& other)
	{
		for (int i = 0; i < nelement; i++)
		{
			e[i] = other.e[i];
		}
	}
	Vec<nelement>& operator=(const Vec<nelement>& other);
	Vec<nelement>& operator=(const float other[])
	{
		for (int i = 0; i < nelement; i++)
		{
			e[i] = other[i];
		}
		return *this;
	}
	float& operator[](int i);
	const float& operator[](int i) const;

	float Norm() const;
	float Norm2() const;
	Vec<nelement> Normalize() const;
	Vec<nelement + 1> AddDimension1(float value);
	template<int n>
	Vec<n> ReduceDimension();

	float e[nelement] = { 0 };
};

template<int nrow, int ncol> 
class Mat
{
public:
	Mat();
	Mat(const Mat<nrow, ncol>& other)
	{
		for (int i = 0; i < nrow; i++)
		{
			for (int j = 0; j < ncol; j++)
			{
				rows[i][j] = other.rows[i][j];
			}
		}
	}
	Mat<nrow, ncol>& operator=(const Mat<nrow, ncol>& other)
	{
		for (int i = 0; i < nrow; i++)
		{
			for (int j = 0; j < ncol; j++)
			{
				rows[i][j] = other.rows[i][j];
			}
		}
		return *this;
	}

	Mat(std::vector<Vec<nrow>>& vecs)
	{
		for(int i = 0; i < ncol; i++)
		{
			SetCol(i, vecs[i]);
		}
	}

	Mat<nrow, ncol>& operator=(const std::vector<Vec<nrow>>& vecs)
	{
		for(int i = 0; i < ncol; i++)
		{
			SetCol(i, vecs[i]);
		}
		return *this;
	}

	Mat<nrow, ncol> AdjugateTranspose() const
	{
		Mat<nrow, ncol> ret;
		for (int i = 0; i < nrow; i++)
		{
			for(int j = 0; j < ncol; j++)
			{
				ret[i][j] = Cofactor(i, j);
			}
		}
		return ret;
	}
	
	
	Vec<ncol>& operator[](int i);
	const Vec<ncol>& operator[](int i) const;

	Mat<nrow, ncol> Transpose() const;
	Mat<nrow, ncol> Invert() const;
	Mat<nrow, ncol> InvertTranspose() const;
	// 余子式
	float Minor(int row, int col) const;
	// 代数余子式
	float Cofactor(int row, int col) const;
	// 行列式
	float Det() const;
	
	static Mat<nrow, ncol> Identity();
	void SetCol(const int idx, const Vec<nrow>& v);
	Vec<nrow> GetCol(const int idx) const;

	Vec<ncol> rows[nrow];
};

// 辅助计算矩阵Det的结构体
template<int n> struct dt
{
	static float Det(const Mat<n, n>& mat)
	{
		float ret = 0;
		for (int i = 0; i < n; i++)
		{
			ret += mat[0][i] * mat.Cofactor(0, i);
		}
		return ret;
	}
};

// 特化版本，结束条件
template<> struct dt<1>
{
	static float Det(const Mat<1, 1>& mat)
	{
		return mat[0][0];
	}
};



template<int nelement>
inline Vec<nelement>& Vec<nelement>::operator=(const Vec<nelement>& other)
{
	for (int i = 0; i < nelement; i++)
	{
		e[i] = other.e[i];
	}
	return *this;
}

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

// 给向量增加一个维度
template<int nelement>
inline Vec<nelement + 1> Vec<nelement>::AddDimension1(float value)
{
	Vec<nelement + 1> vec;
	for (int i = 0; i < nelement; i++)
	{
		vec.e[i] = e[i];
	}
	vec.e[nelement] = value;
	return vec;
}


template<int nrow, int ncol>
inline Mat<nrow, ncol>::Mat()
{
	// rows[nrow] = { {} };
}

//template<int nrow, int ncol>
//inline Mat<nrow, ncol>& Mat<nrow, ncol>::operator=(Mat<nrow, ncol>& other)
//{
//	for (int i = 0; i < nrow; i++)
//	{
//		for (int j = 0; j < ncol; j++)
//		{
//			rows[i][j] = other.rows[i][j];
//		}
//	}
//}

template<int nrow, int ncol>
inline Vec<ncol>& Mat<nrow, ncol>::operator[](int i)
{
	return rows[i]; 
}

template<int nrow, int ncol>
inline const Vec<ncol>& Mat<nrow, ncol>::operator[](int i) const
{
	return rows[i];
}

template<int nrow, int ncol>
inline Mat<nrow, ncol> Mat<nrow, ncol>::Transpose() const
{
	Mat<ncol, nrow> mat;
	for (int i = 0; i < ncol; i++)
	{
		mat.rows[i] = GetCol(i);
	}
	return mat;
}

template<int nrow, int ncol>
inline Mat<nrow, ncol> Mat<nrow, ncol>::Invert() const
{
	return InvertTranspose().Transpose();
}

template<int nrow, int ncol>
inline Mat<nrow, ncol> Mat<nrow, ncol>::InvertTranspose() const
{
	Mat<nrow, ncol> ret;
	ret = AdjugateTranspose(); 
	return ret / (ret[0] * rows[0]);
}

template<int nrow, int ncol>
inline float Mat<nrow, ncol>::Minor(int row, int col) const
{
	Mat<nrow-1, ncol-1> mat;
	for (int i = 0; i < nrow-1; i++)
	{
		for (int j = 0; j < ncol-1; j++)
		{
			int x = i < row ? i : i + 1;
			int y = j < col ? j : j + 1;
			mat.rows[i][j] = rows[x][y];
		}
	}
	return mat.Det();
}

template<int nrow, int ncol>
inline float Mat<nrow, ncol>::Cofactor(int row, int col) const
{
	return Minor(row, col) * pow(-1, (row+col));
}

template<int nrow, int ncol>
inline float Mat<nrow, ncol>::Det() const
{
	assert(nrow == ncol);
	return dt<nrow>::Det(*this);
}

template<int nrow, int ncol>
inline Mat<nrow, ncol> Mat<nrow, ncol>::Identity()
{
	Mat<nrow, ncol> mat;
	assert(nrow == ncol);
	for (int i = 0; i < nrow; ++i)
		for (int j = 0; j < ncol; ++j)
			mat[i][j] = (i == j);
	return mat;
}

// 将向量v设置到矩阵Mat的第idx列
template<int nrow, int ncol>
inline void Mat<nrow, ncol>::SetCol(const int idx, const Vec<nrow>& v)
{
	for (int i = 0; i < nrow; i++)
	{
		rows[i][idx] = v.e[i];
	}
}

// Vec的类型声明要在Mat前面，这里才不会报错
template<int nrow, int ncol>
inline Vec<nrow> Mat<nrow, ncol>::GetCol(const int idx) const
{
	Vec<nrow> vec;
	for (int i = 0; i < nrow; i++)
	{
		vec.e[i] = rows[i][idx];
	}
	return vec;
}





// 双目运算，不定义为类成员函数

// Vec
/*template<int nelement>
Vec<nelement> operator+(const Vec<nelement>& m, const Vec<nelement>& n);
template<int nelement>
Vec<nelement> operator-(const Vec<nelement>& m, const Vec<nelement>& n);
template<int nelement>
Vec<nelement> operator*(const Vec<nelement>& m, const Vec<nelement>& n);
template<int nelement>
Vec<nelement> operator*(float t, const Vec<nelement>& n);
template<int nelement>
Vec<nelement> operator*(const Vec<nelement>& n, float t);
template<int nelement>
Vec<nelement> operator/(Vec<nelement>& n, float t);*/



template<int nelement>
float operator*(const Vec<nelement>& lhs, const Vec<nelement>& rhs)
{
	float ret = 0;
	for (int i = 0; i < nelement; i++)
	{
		ret += lhs[i] * rhs[i];
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
	Vec<nelement> vec = { 0 };
	for (int i = 0; i < nelement; i++)
	{
		vec.e[i] = lhs[i] + rhs[i];
	}
	return vec;
}

template<int nelement>
Vec<nelement> operator-(const Vec<nelement>& lhs, const Vec<nelement>& rhs)
{
	Vec<nelement> vec;
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
		vec.e[i] = lhs.e[i] / m;
	}
	return vec;
}


typedef Vec<3> Vec3;
typedef Vec<2> Vec2;

float Dot(const Vec3& m, const Vec3& n);

Vec3 Cross(const Vec3& m, const Vec3& n);

template<typename T>
T UniformVec(T& Vec);

template<typename T>
inline T UniformVec(T& Vec)
{
	return Vec / Vec.Norm();
}

// 矩阵乘向量
template<int nrow, int ncol, int ncol1>
Vec<nrow> operator* (const Mat<nrow, ncol>& mat, const Vec<ncol1>& vec)
{
	Vec<nrow> ret;
	for (int i = 0; i < nrow; i++)
	{
		ret[i] = mat.rows[i] * vec;
	}
	return ret;
}

template<int nrow, int ncol, int ncol1>
Mat<nrow, ncol1> operator* (const Mat<nrow, ncol>& mat, const Mat<ncol, ncol1>& mat1)
{
	Mat<nrow, ncol1> ret;
	for (int c = 0; c < ncol1; c++)
	{
		Vec<nrow> vec;
		for (int i = 0; i < nrow; i++)
		{
			vec[i] = mat.rows[i] * mat1.GetCol(c);
		}
		ret.SetCol(c, vec);
	}
	return ret;
}

template <int nrow, int ncol>
Mat<nrow, ncol> operator / (const Mat<nrow, ncol>& mat, const float rhs)
{
	Mat<nrow, ncol> ret(mat);
	for(int i = 0; i < nrow; i++)
	{
		for (int j = 0; j < ncol; j++)
		{
			ret[i][j] = mat[i][j] / rhs;
		}
	}
	return ret;
}

template<int nelement>
template<int n>
inline Vec<n> Vec<nelement>::ReduceDimension()
{
	Vec<n> ret;
	for (int i = 0; i < n; i++)
	{
		ret[i] = e[i];
	}
	return ret;
}
