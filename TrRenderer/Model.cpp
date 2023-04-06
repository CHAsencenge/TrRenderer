#include "Model.h"

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
