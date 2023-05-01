#pragma once
#include <vector>
#include "Maths.h"
#include "TGA.h"

// 前向声明，Shader.h中已经include了Model.h，这里防止相互包含
typedef struct Cubemap Cubemap_t;

class Model
{
public:
	Model(const char* filename);
	~Model();

	Cubemap_t* environmentMap;

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

	void LoadTexture(const char* filename, const char* suffix, TGAImage& img);
	// 通过tga文件将各个贴图读到对应贴图数据成员中
	void CreateMap(const char* filename);

	int GetNumVerts();
	int GetNumFaces();
	
	Vec3 GetDiffuseByUV(Vec2 uv);
	float GetRoughnessByUV(Vec2 uv);
	float GetMetalnessByUV(Vec2 uv);
	Vec3 GetEmissionByUV(Vec2 uv);
	float GetOcclusionByUV(Vec2 uv);
	float GetSpecularByUV(Vec2 uv);

	std::vector<int> GetFaceByIndex(int idx);

private:
	std::vector<Vec3> verts;
	std::vector<std::vector<int>> faces;
	std::vector<Vec3> norms;
	std::vector<Vec2> uvs;
};
