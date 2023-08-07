#include "pch.h"
#include "Shader.h"

extern Mat<4, 4> ModelView;
extern Mat<4, 4> Projection;


// 用floot函数将float类型的采样点坐标向下取整
TGAColor IShader::Sample2D(TGAImage& img, Vec2 uv)
{
	return img.Get(floor(uv.e[0] * img.Width()), floor(uv.e[1] * img.Height()));
}


DefaultShader::DefaultShader(const Model& m, Vec3& lightDir) : model(m)
{
	uniformLightDir = (ModelView * lightDir.AddDimension1(0.0f)).ReduceDimension<3>().Normalize();
}



void DefaultShader::VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position)
{
	// 在调用VertexShader时，从model中获取顶点的UV和法线等信息来构造相应矩阵
	matVertexUVs.SetCol(nthVert, model.UV(ithFace, nthVert));
	matVertexNormals.SetCol(nthVert, model.Normal(ithFace, nthVert));
	std::vector<int> faceVertsIdx = model.FaceVert(ithFace);
	Vec3 vert = model.Vert(faceVertsIdx[nthVert]);
	VertexShader(vert, gl_Position);
}

void DefaultShader::VertexShader(Vec3 vert, Vec<4>& gl_Position)
{
	Vec<4> vertHomo = vert.AddDimension1(1);
	Vec<4> vertEyeSpace = ModelView * vertHomo;
	Vec<4> vertClipSpace = Projection * vertEyeSpace;
	gl_Position = vertClipSpace;
}

// 输入：像素点对应Shader中三个顶点构成的三角形的重心坐标
bool DefaultShader::FragmentShader(const Vec3 barycenter, TGAColor& gl_FragColor)
{
	// 法线插值
	Vec<3> fragNormalTS = (matVertexNormals * barycenter).Normalize();
	// UV插值
	Vec<2> fragUV = matVertexUVs * barycenter;
	// 将法线从tangent space通过TBN矩阵转到world space
	Mat<3, 3> matVertexNormalsWS;
	matVertexNormalsWS.SetCol(0, TBN[0] * matVertexNormals.GetCol(0));
	matVertexNormalsWS.SetCol(1, TBN[1] * matVertexNormals.GetCol(1));
	matVertexNormalsWS.SetCol(2, TBN[2] * matVertexNormals.GetCol(2));
	Vec<3> fragNormalWS = (matVertexNormalsWS * barycenter).Normalize();
	// 计算漫反射光照强度，反射光照向量，高光强度
	// 通过插值后的uv采样颜色贴图中的颜色
	// 计算叠加光照效果后的颜色
	
	// 返回值确定此片元是否被丢弃
	return false;
}

void DefaultShader::ComputeTBN()
{
	Vec<3> Vert0 = matVertexVerts.GetCol(0);
	Vec<3> Vert1 = matVertexVerts.GetCol(1);
	Vec<3> Vert2 = matVertexVerts.GetCol(2);
	Vec<2> UV0 = matVertexUVs.GetCol(0);
	Vec<2> UV1 = matVertexUVs.GetCol(1);
	Vec<2> UV2 = matVertexUVs.GetCol(2);

	Mat<2, 2> deltaUV;
	deltaUV.SetCol(0, UV1-UV0);
	deltaUV.SetCol(1, UV2-UV0);

	Mat<3, 2> deltaPos;
	deltaPos.SetCol(0, Vert1-Vert0);
	deltaPos.SetCol(1, Vert2-Vert0);

	Mat<3, 2> TB = deltaPos * deltaUV.Inverse();

	for (int v = 0; v < 3; v++)
	{
		Vec<3> T = TB.GetCol(0) - matVertexNormals.GetCol(v) * Dot(matVertexNormals.GetCol(v), TB.GetCol(0));
		Vec<3> B = Cross(matVertexNormals.GetCol(v), T);
		Mat<3, 3> res;
		res.SetCol(0, T);
		res.SetCol(1, B);
		res.SetCol(2, matVertexNormals.GetCol(v));
		TBN.push_back(res);
	}
}

