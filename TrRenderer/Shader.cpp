#include "pch.h"
#include "Shader.h"

extern Mat<4, 4> ModelView;
extern Mat<4, 4> Projection;


// 用floot函数将float类型的采样点坐标向下取整
TGAColor IShader::Sample2D(TGAImage& img, Vec2 uv)
{
	return img.Get(floor(uv.e[0] * img.Width()), floor(uv.e[1] * img.Height()));
}


DefaultShader::DefaultShader(Model& m, Vec3& lightDir) : model(&m)
{
	uniformLightDir = (ModelView * lightDir.AddDimension1(0.0f)).ReduceDimension<3>().Normalize();
}



void DefaultShader::VertexShader(int ithFace, int nthVert, Vec<4>& gl_Position)
{
	// 在调用VertexShader时，从model中获取顶点的UV和法线等信息来构造相应矩阵
	matVertexUVs.SetCol(nthVert, model->UV(ithFace, nthVert));
	
	// 顶点法线从模型空间转到视图空间
	Vec<4> vertNormalAD = model->Normal(ithFace, nthVert).AddDimension1(0);
	matVertexNormals.SetCol(nthVert, (ModelView * vertNormalAD).ReduceDimension<3>());
	
	std::vector<int> faceVertsIdx = model->FaceVert(ithFace);
	Vec3 vert = model->Vert(faceVertsIdx[nthVert]);
	Vec<4> vertHomo = vert.AddDimension1(1);
	Vec<4> vertEyeSpace = ModelView * vertHomo;

	// 记录的顶点坐标在视图空间
	matVertexVerts.SetCol(nthVert, vertEyeSpace.ReduceDimension<3>());
	Vec<4> vertClipSpace = Projection * vertEyeSpace;
	gl_Position = vertClipSpace;
}

void DefaultShader::VertexShader(Vec3 vert, Vec<4>& gl_Position)
{
	
}

// 输入：像素点对应Shader中三个顶点构成的三角形的重心坐标
bool DefaultShader::FragmentShader(const Vec3 barycenter, TGAColor& gl_FragColor)
{
	// Eye Space的顶点法线
	Vec<3> fragNormalES = (matVertexNormals * barycenter).Normalize();
	
	// UV插值
	Vec<2> fragUV = matVertexUVs * barycenter;

	// 将法线从tangent space转到view space
	// 要找到一个向量，这个向量首先正交于view space法线方向
	// 然后这个向量向UV方向投影拆分成两个轴向量t和b
	// 再同法线向量合并成tbn
	// x = A^(-1) b
	Mat<3, 3> InvertA;
	InvertA.SetCol(0, matVertexVerts.GetCol(1) - matVertexVerts.GetCol(0));
	InvertA.SetCol(1, matVertexVerts.GetCol(2) - matVertexVerts.GetCol(0));
	InvertA.SetCol(2, fragNormalES);
	InvertA = InvertA.Inverse();
	
	Mat<3, 2> UVDiff;
	UVDiff.SetCol(0, (matVertexUVs.GetCol(1) - matVertexUVs.GetCol(0)).AddDimension1(0));
	UVDiff.SetCol(1, (matVertexUVs.GetCol(2) - matVertexUVs.GetCol(0)).AddDimension1(0));

	Mat<3, 2> ij = InvertA * UVDiff;
	Mat<3, 3> TBN;
	TBN.SetCol(0, ij.GetCol(0).Normalize());
	TBN.SetCol(1, ij.GetCol(1).Normalize());
	TBN.SetCol(2, fragNormalES);

	// 从法线贴图中采法线值出来，先映射到[-1, 1]，再转换到eye space
	Vec<3> SampleNormalmapES = (TBN * model->NormalByUV(fragUV)).Normalize();

	// 计算漫反射光照强度，反射光照向量，高光强度
	// 通过插值后的uv采样颜色贴图中的颜色
	// 分通道计算叠加光照效果后的颜色
	float diffuseIntensity = std::max(0.0f, Dot(SampleNormalmapES, uniformLightDir));
	Vec<3> reflectDir = SampleNormalmapES * (Dot(SampleNormalmapES, uniformLightDir) * 2) - uniformLightDir;
	float specularIntensity;
	if (model->GetSpecularMap() != nullptr)
	{
		specularIntensity = std::pow(std::max(-reflectDir.e[2], 0.0f), 5 + Sample2D(*model->GetSpecularMap(), fragUV).bgra[0]);
	}
	else
	{
		specularIntensity = std::pow(std::max(-reflectDir.e[2], 0.0f), 5);
	}
	TGAColor baseColor = Sample2D(*model->GetDiffuseMap(), fragUV);
	for(int i = 0; i < 3; i++)
	{
		gl_FragColor.bgra[i] = std::min<int>(10 + baseColor.bgra[i] * (diffuseIntensity + specularIntensity), 255);
	}
	
	// 返回值确定此片元是否被丢弃
	return false;
}


