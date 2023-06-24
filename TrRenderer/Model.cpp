#include "pch.h"
#include "Model.h"
// #include <fstream>
#include <sstream>

// 读取obj文件
// 读取diffuse等纹理
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
		// uv
		else if (!line.compare(0, 3, "vt "))
		{
			iss >> trash >> trash;
			Vec2 uv;
			for (int i = 0; i < 2; i++)
			{
				iss >> uv[i];
			}
			uvs.push_back(uv);
		}
		// face行结构
		// f v1 / vt1 / vn1 v2 / vt2 / vn2 v3 / vt3 / vn3
		else if (!line.compare(0, 2, "f "))
		{
			iss >> trash;
			int f[3];
			std::vector<int> face;
			int count = 0;
			// 一般来说一行会while3次
			while (iss >> f[0] >> trash >> f[1] >> trash >> f[2] >> trash)
			{
				count++;
				printf("Model::Model face line while count %d\n", count);
				face.push_back(f[0]);
				face.push_back(f[1]);
				face.push_back(f[2]);
			}
			faces.push_back(face);
		}
	}

	CreateMap(filename);
}

Model::~Model()
{
}

Vec3 Model::Vert(int idx)
{
	return verts[idx];
}

Vec3 Model::Vert(int iFace, int nthVert)
{
	return Vec3();
}

std::vector<int> Model::FaceVert(int idx)
{
	// 根据face line结构
	std::vector<int> face = faces[idx];
	std::vector<int> vert;
	int i = 0;
	while (i < face.size())
	{
		vert.push_back(face[i]);
		i += 3;
	}
	return vert;
}

Vec2 Model::UV(int ithFace, int nthVert)
{
	assert(nthVert < 3);
	std::vector<int> face = faces[ithFace];
	int uvIdx = face[1 + nthVert * 3];
	Vec2 uv = uvs[uvIdx];
	return uv;
}

// diffuse, normal, spec, tangent等纹理读取
// 主要是加后缀找到相应文件，实际读取调用ReadTGAFile
// tangent怎么用
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
	// 读取diffuse等纹理
	LoadTexture(filename, "_diffuse.tga", *diffuseMap);
	LoadTexture(filename, "_nm_tangent.tga", *normalMap);
	LoadTexture(filename, "_spec.tga", *specularMap);
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
