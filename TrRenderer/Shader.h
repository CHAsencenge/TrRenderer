#pragma once
#include "Model.h"

typedef struct Cubemap
{
	TGAImage* faces[6];
}Cubemap_t;

class Shader
{
public:
	Shader(Model& model);
	~Shader();

	void VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position);
	void FragmentShader();

private:



};