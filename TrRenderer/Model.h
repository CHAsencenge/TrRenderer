#pragma once
#include <vector>
#include "Maths.h"
#include "TGA.h"

// 前向声明，Shader.h中已经include了Model.h，这里防止相互包含
typedef struct Cubemap Cubemap_t;

class Model
{
public:

	TGAImage* diffuseMap;
	TGAImage* normalMap;
	TGAImage* specularMap;
	TGAImage* roughnessMap;
	TGAImage* metalnessMap;
	TGAImage* occlusionMap;
	TGAImage* emissionMap;

	Vec3 Vert(int i);
	Vec3 Vert(int iFace, int nthVert);
	std::vector<int> Face(int idx);

	void LoadTexture(std::string filename, const char* suffix, TGAImage& img);

private:
	std::vector<Vec3> verts;
	std::vector<std::vector<int>> faces;
	std::vector<Vec3> norms;
	std::vector<Vec2> uvs;
};
