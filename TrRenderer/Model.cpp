#include "Model.h"
// #include <fstream>
#include <sstream>


Model::Model(const char* filename)
{
	std::ifstream in;
	in.open(filename, std::ifstream::in);
	if (in.fail())
	{
		printf("Model::Model load model failed\n");
		return;
	}

	std::string line;
	// eof()判断文件是否已经读取到了文件末尾
	while (!in.eof())
	{
		std::getline(in, line);

		// 从字符串中读取数据的输入流
		std::istringstream iss(line.c_str());
		char trash;
		// 如果当前字符串等于参数字符串，则返回0
		// 如果当前字符串大于参数字符串，则返回一个正整数
		// 如果当前字符串小于参数字符串，则返回一个负整数
		if (!line.compare(0, 2, "v "))
		{
			// 去掉开头的v
			iss >> trash;
			Vec3 v;
			// 顶点xyz?
			for (int i = 0; i < 3; i++)
			{
				iss >> v[i];
			}
			verts.push_back(v);
		}
		else if (!line.compare(0, 3, "vn "))
		{
			// 去掉开头的vn
			iss >> trash >> trash;
			Vec3 n;
			for (int i = 0; i < 3; i++)
			{
				iss >> n[i];
			}
			norms.push_back(n);
		}
	}
}

Model::~Model()
{
}

Vec3 Model::Vert(int i)
{
	return Vec3();
}

Vec3 Model::Vert(int iFace, int nthVert)
{
	return Vec3();
}

std::vector<int> Model::Face(int idx)
{
	return std::vector<int>();
}

void Model::LoadTexture(const char* filename, const char* suffix, TGAImage& img)
{
	std::string texFile(filename);
	size_t dot = texFile.find_last_of(".");
	if (dot != std::string::npos)
	{
		// suffix要带.tga后缀
		texFile = texFile.substr(0, dot) + std::string(suffix);
		// 读纹理到img
		img.ReadTGAFile(texFile.c_str());
	}
}

void Model::CreateMap(const char* filename)
{
}

int Model::GetNumVerts()
{
	return 0;
}

int Model::GetNumFaces()
{
	return 0;
}

Vec3 Model::GetDiffuseByUV(Vec2 uv)
{
	return Vec3();
}

float Model::GetRoughnessByUV(Vec2 uv)
{
	return 0.0f;
}

float Model::GetMetalnessByUV(Vec2 uv)
{
	return 0.0f;
}

Vec3 Model::GetEmissionByUV(Vec2 uv)
{
	return Vec3();
}

float Model::GetOcclusionByUV(Vec2 uv)
{
	return 0.0f;
}

float Model::GetSpecularByUV(Vec2 uv)
{
	return 0.0f;
}

std::vector<int> Model::GetFaceByIndex(int idx)
{
	return std::vector<int>();
}
