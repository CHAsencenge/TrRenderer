#include "TrVulkanModel.h"

#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

TrVulkanModelBase::TrVulkanModelBase(const std::string& filename, const std::string& modelPath, const std::string& texturePath) :
mFilename(filename),
mModelPath(modelPath),
mTexturePath(texturePath)
{
    LoadModelFromPath();
}

void TrVulkanModelBase::LoadModelFromPath()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes; // a surface, contains a vertex array, each vertex contains loc index, normal index, tex index
    std::vector<tinyobj::material_t> materials; // obj file allows defining material for each surface
    std::string warn, err;

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, mModelPath.c_str()))
    {
        throw std::runtime_error(err);
    }

    std::unordered_map<TrVulkanVertex3DTex, uint32_t> uniqueVertices = {};
    for(const auto& shape : shapes)
    {
        for(const auto& index : shape.mesh.indices)
        {
            TrVulkanVertex3DTex TrVertex = {};

            TrVertex.mPos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2],
            };

            TrVertex.mTexCoord =
            {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
            };

            TrVertex.mColor = {1.0f, 1.0f, 1.0f};

            if(uniqueVertices.count(TrVertex) == 0)
            {
                uniqueVertices[TrVertex] = static_cast<uint32_t>(mVertices.size());
                mVertices.push_back(TrVertex);
            }

            mIndices.push_back(uniqueVertices[TrVertex]);
        }
    }
    
}
