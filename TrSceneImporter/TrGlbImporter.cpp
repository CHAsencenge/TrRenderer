#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "TrGlbImporter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
    const char* ResultName(cgltf_result result)
    {
        switch(result)
        {
            case cgltf_result_success: return "success";
            case cgltf_result_data_too_short: return "data too short";
            case cgltf_result_unknown_format: return "unknown format";
            case cgltf_result_invalid_json: return "invalid JSON";
            case cgltf_result_invalid_gltf: return "invalid glTF";
            case cgltf_result_invalid_options: return "invalid options";
            case cgltf_result_file_not_found: return "file not found";
            case cgltf_result_io_error: return "I/O error";
            case cgltf_result_out_of_memory: return "out of memory";
            case cgltf_result_legacy_gltf: return "legacy glTF";
            default: return "unknown error";
        }
    }

    std::string NameOrFallback(const char* name, const char* prefix, std::size_t index)
    {
        return name != nullptr && name[0] != '\0'
            ? std::string(name)
            : std::string(prefix) + std::to_string(index);
    }

    template<typename T>
    std::uint32_t IndexOf(const T* value, const T* base, std::size_t count)
    {
        if(value == nullptr)
        {
            return TrInvalidSceneIndex;
        }
        if(base == nullptr || value < base || value >= base + count)
        {
            throw std::runtime_error("GLB contains an invalid object reference.");
        }
        const std::size_t index = static_cast<std::size_t>(value - base);
        if(index > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error("GLB object index exceeds 32 bits.");
        }
        return static_cast<std::uint32_t>(index);
    }

    std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if(!stream)
        {
            throw std::runtime_error("Failed to open input GLB file.");
        }
        const std::streamoff size = stream.tellg();
        if(size <= 0 || static_cast<std::uint64_t>(size) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::runtime_error("Input GLB file size is invalid.");
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char*>(bytes.data()), size);
        if(!stream)
        {
            throw std::runtime_error("Failed to read input GLB file.");
        }
        return bytes;
    }

    const cgltf_accessor* FindAttribute(
        const cgltf_primitive& primitive,
        cgltf_attribute_type type,
        int attributeIndex = 0)
    {
        for(cgltf_size index = 0; index < primitive.attributes_count; ++index)
        {
            const cgltf_attribute& attribute = primitive.attributes[index];
            if(attribute.type == type && attribute.index == attributeIndex)
            {
                return attribute.data;
            }
        }
        return nullptr;
    }

    std::vector<float> ReadAccessorFloats(const cgltf_accessor* accessor)
    {
        if(accessor == nullptr)
        {
            return {};
        }
        const cgltf_size componentCount = cgltf_num_components(accessor->type);
        if(componentCount == 0 || accessor->count >
            std::numeric_limits<std::size_t>::max() / componentCount)
        {
            throw std::runtime_error("GLB accessor dimensions are invalid.");
        }
        std::vector<float> values(accessor->count * componentCount);
        const cgltf_size unpacked = cgltf_accessor_unpack_floats(
            accessor,
            values.data(),
            values.size());
        if(unpacked != values.size())
        {
            throw std::runtime_error("Failed to unpack a GLB vertex accessor.");
        }
        return values;
    }

    std::vector<std::uint32_t> ReadIndices(
        const cgltf_accessor* accessor,
        std::size_t vertexCount)
    {
        if(accessor == nullptr)
        {
            if(vertexCount > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::overflow_error("Non-indexed GLB primitive exceeds 32-bit indices.");
            }
            std::vector<std::uint32_t> indices(vertexCount);
            for(std::size_t index = 0; index < vertexCount; ++index)
            {
                indices[index] = static_cast<std::uint32_t>(index);
            }
            return indices;
        }

        if(accessor->count > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error("GLB index count exceeds 32 bits.");
        }
        std::vector<std::uint32_t> indices(accessor->count);
        const cgltf_size unpacked = cgltf_accessor_unpack_indices(
            accessor,
            indices.data(),
            sizeof(std::uint32_t),
            indices.size());
        if(unpacked != indices.size())
        {
            throw std::runtime_error("Failed to unpack a GLB index accessor.");
        }
        for(std::uint32_t index : indices)
        {
            if(index >= vertexCount)
            {
                throw std::runtime_error("GLB primitive contains an out-of-range index.");
            }
        }
        return indices;
    }

    void Normalize(std::array<float, 3>& value)
    {
        const float lengthSquared =
            value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
        if(lengthSquared > 1.0e-20f)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            value[0] *= inverseLength;
            value[1] *= inverseLength;
            value[2] *= inverseLength;
        }
        else
        {
            value = {0.0f, 1.0f, 0.0f};
        }
    }

    void GenerateNormals(
        std::vector<TrSceneVertex>& vertices,
        std::uint32_t firstVertex,
        const std::vector<std::uint32_t>& localIndices)
    {
        for(std::size_t index = 0; index + 2 < localIndices.size(); index += 3)
        {
            TrSceneVertex& a = vertices[firstVertex + localIndices[index + 0]];
            TrSceneVertex& b = vertices[firstVertex + localIndices[index + 1]];
            TrSceneVertex& c = vertices[firstVertex + localIndices[index + 2]];
            const std::array<float, 3> ab =
            {
                b.Position[0] - a.Position[0],
                b.Position[1] - a.Position[1],
                b.Position[2] - a.Position[2]
            };
            const std::array<float, 3> ac =
            {
                c.Position[0] - a.Position[0],
                c.Position[1] - a.Position[1],
                c.Position[2] - a.Position[2]
            };
            const std::array<float, 3> normal =
            {
                ab[1] * ac[2] - ab[2] * ac[1],
                ab[2] * ac[0] - ab[0] * ac[2],
                ab[0] * ac[1] - ab[1] * ac[0]
            };
            for(std::size_t axis = 0; axis < 3; ++axis)
            {
                a.Normal[axis] += normal[axis];
                b.Normal[axis] += normal[axis];
                c.Normal[axis] += normal[axis];
            }
        }
        const std::uint32_t vertexCount = static_cast<std::uint32_t>(
            vertices.size() - firstVertex);
        for(std::uint32_t index = 0; index < vertexCount; ++index)
        {
            Normalize(vertices[firstVertex + index].Normal);
        }
    }

    TrSceneTextureBinding ConvertTextureBinding(
        const cgltf_texture_view& view,
        const cgltf_data& data)
    {
        TrSceneTextureBinding binding;
        if(view.texture == nullptr)
        {
            return binding;
        }
        binding.TextureIndex = static_cast<std::int32_t>(IndexOf(
            view.texture,
            data.textures,
            data.textures_count));
        binding.TexCoord = view.texcoord;
        binding.Strength = view.scale;
        if(view.has_transform)
        {
            binding.Offset = {view.transform.offset[0], view.transform.offset[1]};
            binding.Scale = {view.transform.scale[0], view.transform.scale[1]};
            binding.Rotation = view.transform.rotation;
            if(view.transform.has_texcoord)
            {
                binding.TexCoord = view.transform.texcoord;
            }
        }
        return binding;
    }

    std::array<float, 16> ConvertMatrix(const float* gltfMatrix)
    {
        // glTF stores column-major matrices and uses a right-handed coordinate
        // system. Tr stores row-major matrices for row-vector math and flips Z.
        constexpr float sign[4] = {1.0f, 1.0f, -1.0f, 1.0f};
        std::array<float, 16> result = {};
        for(std::size_t row = 0; row < 4; ++row)
        {
            for(std::size_t column = 0; column < 4; ++column)
            {
                result[row * 4 + column] =
                    sign[row] * sign[column] * gltfMatrix[row * 4 + column];
            }
        }
        return result;
    }

    TrSceneAlphaMode ConvertAlphaMode(cgltf_alpha_mode mode)
    {
        switch(mode)
        {
            case cgltf_alpha_mode_mask: return TrSceneAlphaMode::Mask;
            case cgltf_alpha_mode_blend: return TrSceneAlphaMode::Blend;
            default: return TrSceneAlphaMode::Opaque;
        }
    }
}

TrGlbImportResult TrGlbImporter::Import(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if(extension != ".glb")
    {
        throw std::invalid_argument("Tr GLB importer accepts only .glb files.");
    }

    const std::vector<std::uint8_t> fileBytes = ReadFile(path);
    cgltf_options options = {};
    cgltf_data* parsedData = nullptr;
    const cgltf_result parseResult = cgltf_parse(
        &options,
        fileBytes.data(),
        fileBytes.size(),
        &parsedData);
    if(parseResult != cgltf_result_success || parsedData == nullptr)
    {
        throw std::runtime_error(
            std::string("Failed to parse GLB container: ") + ResultName(parseResult) + ".");
    }
    const std::unique_ptr<cgltf_data, void(*)(cgltf_data*)> data(parsedData, cgltf_free);
    if(data->file_type != cgltf_file_type_glb)
    {
        throw std::runtime_error("Input file is not a binary glTF 2.0 container.");
    }

    const std::string utf8Path = path.u8string();
    const cgltf_result bufferResult = cgltf_load_buffers(
        &options,
        data.get(),
        utf8Path.c_str());
    if(bufferResult != cgltf_result_success)
    {
        throw std::runtime_error(
            std::string("Failed to load GLB binary buffers: ") + ResultName(bufferResult) + ".");
    }
    if(cgltf_validate(data.get()) != cgltf_result_success)
    {
        throw std::runtime_error("GLB failed structural validation.");
    }
    for(cgltf_size viewIndex = 0; viewIndex < data->buffer_views_count; ++viewIndex)
    {
        if(data->buffer_views[viewIndex].has_meshopt_compression)
        {
            throw std::runtime_error(
                "Meshopt-compressed GLB buffer views are not supported.");
        }
    }

    TrGlbImportResult result;
    TrScene& scene = result.Scene;
    scene.Name = path.stem().u8string();
    if(data->asset.generator != nullptr)
    {
        scene.SourceGenerator = data->asset.generator;
    }

    scene.Images.reserve(data->images_count);
    for(cgltf_size imageIndex = 0; imageIndex < data->images_count; ++imageIndex)
    {
        const cgltf_image& source = data->images[imageIndex];
        TrSceneImage image;
        image.Name = NameOrFallback(source.name, "Image_", imageIndex);
        image.MimeType = source.mime_type != nullptr ? source.mime_type : "application/octet-stream";
        if(source.buffer_view != nullptr && source.buffer_view->buffer != nullptr &&
           source.buffer_view->buffer->data != nullptr)
        {
            const cgltf_buffer_view& view = *source.buffer_view;
            if(view.offset > view.buffer->size ||
               view.size > view.buffer->size - view.offset)
            {
                throw std::runtime_error("GLB image buffer view is out of range.");
            }
            const auto* begin = static_cast<const std::uint8_t*>(view.buffer->data) + view.offset;
            image.Data.assign(begin, begin + view.size);
        }
        else
        {
            result.Warnings.push_back(
                "Image '" + image.Name + "' is not embedded in the GLB and was not copied.");
        }
        scene.Images.push_back(std::move(image));
    }

    scene.Samplers.reserve(data->samplers_count);
    for(cgltf_size samplerIndex = 0; samplerIndex < data->samplers_count; ++samplerIndex)
    {
        const cgltf_sampler& source = data->samplers[samplerIndex];
        TrSceneSampler sampler;
        sampler.Name = NameOrFallback(source.name, "Sampler_", samplerIndex);
        if(source.mag_filter != cgltf_filter_type_undefined)
        {
            sampler.MagFilter = static_cast<std::uint32_t>(source.mag_filter);
        }
        if(source.min_filter != cgltf_filter_type_undefined)
        {
            sampler.MinFilter = static_cast<std::uint32_t>(source.min_filter);
        }
        // cgltf applies the glTF REPEAT defaults for omitted wrap modes.
        sampler.WrapU = static_cast<std::uint32_t>(source.wrap_s);
        sampler.WrapV = static_cast<std::uint32_t>(source.wrap_t);
        scene.Samplers.push_back(std::move(sampler));
    }

    scene.Textures.reserve(data->textures_count);
    for(cgltf_size textureIndex = 0; textureIndex < data->textures_count; ++textureIndex)
    {
        const cgltf_texture& source = data->textures[textureIndex];
        TrSceneTexture texture;
        texture.Name = NameOrFallback(source.name, "Texture_", textureIndex);
        const cgltf_image* image = source.image != nullptr
            ? source.image
            : (source.has_basisu ? source.basisu_image : source.webp_image);
        const std::uint32_t imageIndex = IndexOf(image, data->images, data->images_count);
        const std::uint32_t samplerIndex = IndexOf(source.sampler, data->samplers, data->samplers_count);
        texture.ImageIndex = imageIndex == TrInvalidSceneIndex ? -1 : static_cast<std::int32_t>(imageIndex);
        texture.SamplerIndex = samplerIndex == TrInvalidSceneIndex ? -1 : static_cast<std::int32_t>(samplerIndex);
        scene.Textures.push_back(std::move(texture));
    }

    scene.Materials.reserve(data->materials_count);
    for(cgltf_size materialIndex = 0; materialIndex < data->materials_count; ++materialIndex)
    {
        const cgltf_material& source = data->materials[materialIndex];
        TrSceneMaterial material;
        material.Name = NameOrFallback(source.name, "Material_", materialIndex);
        if(source.has_pbr_metallic_roughness)
        {
            const cgltf_pbr_metallic_roughness& pbr = source.pbr_metallic_roughness;
            std::copy_n(pbr.base_color_factor, 4, material.BaseColorFactor.begin());
            material.MetallicFactor = pbr.metallic_factor;
            material.RoughnessFactor = pbr.roughness_factor;
            material.BaseColorTexture = ConvertTextureBinding(pbr.base_color_texture, *data);
            material.MetallicRoughnessTexture = ConvertTextureBinding(
                pbr.metallic_roughness_texture,
                *data);
        }
        std::copy_n(source.emissive_factor, 3, material.EmissiveFactor.begin());
        material.EmissiveStrength = source.has_emissive_strength
            ? source.emissive_strength.emissive_strength
            : 1.0f;
        material.AlphaMode = ConvertAlphaMode(source.alpha_mode);
        material.AlphaCutoff = source.alpha_cutoff;
        material.DoubleSided = source.double_sided != 0;
        material.Unlit = source.unlit != 0;
        material.NormalTexture = ConvertTextureBinding(source.normal_texture, *data);
        material.OcclusionTexture = ConvertTextureBinding(source.occlusion_texture, *data);
        material.EmissiveTexture = ConvertTextureBinding(source.emissive_texture, *data);
        if(source.has_pbr_specular_glossiness || source.has_clearcoat ||
           source.has_transmission || source.has_volume || source.has_ior ||
           source.has_specular || source.has_sheen || source.has_iridescence ||
           source.has_diffuse_transmission || source.has_anisotropy ||
           source.has_dispersion)
        {
            result.Warnings.push_back(
                "Material '" + material.Name + "' contains extensions not used by the current Tr renderer.");
        }
        scene.Materials.push_back(std::move(material));
    }

    scene.Meshes.reserve(data->meshes_count);
    for(cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
    {
        const cgltf_mesh& sourceMesh = data->meshes[meshIndex];
        TrSceneMesh mesh;
        mesh.Name = NameOrFallback(sourceMesh.name, "Mesh_", meshIndex);
        for(cgltf_size primitiveIndex = 0;
            primitiveIndex < sourceMesh.primitives_count;
            ++primitiveIndex)
        {
            const cgltf_primitive& source = sourceMesh.primitives[primitiveIndex];
            if(source.type != cgltf_primitive_type_triangles)
            {
                result.Warnings.push_back(
                    "Mesh '" + mesh.Name + "' contains a non-triangle primitive that was skipped.");
                continue;
            }
            if(source.has_draco_mesh_compression)
            {
                throw std::runtime_error("Draco-compressed GLB primitives are not supported.");
            }

            const cgltf_accessor* positions = FindAttribute(source, cgltf_attribute_type_position);
            if(positions == nullptr || positions->type != cgltf_type_vec3 || positions->count == 0)
            {
                throw std::runtime_error("GLB triangle primitive has no POSITION vec3 accessor.");
            }
            if(positions->count > std::numeric_limits<std::uint32_t>::max() ||
               mesh.Vertices.size() + positions->count > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::overflow_error("GLB mesh exceeds 32-bit vertex addressing.");
            }
            const std::uint32_t firstVertex = static_cast<std::uint32_t>(mesh.Vertices.size());
            const std::uint32_t vertexCount = static_cast<std::uint32_t>(positions->count);
            const std::vector<float> positionValues = ReadAccessorFloats(positions);
            const cgltf_accessor* normals = FindAttribute(source, cgltf_attribute_type_normal);
            const cgltf_accessor* tangents = FindAttribute(source, cgltf_attribute_type_tangent);
            const cgltf_accessor* texCoords = FindAttribute(source, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* texCoords1 = FindAttribute(source, cgltf_attribute_type_texcoord, 1);
            const cgltf_accessor* colors = FindAttribute(source, cgltf_attribute_type_color, 0);
            if((normals != nullptr && normals->type != cgltf_type_vec3) ||
               (tangents != nullptr && tangents->type != cgltf_type_vec4) ||
               (texCoords != nullptr && texCoords->type != cgltf_type_vec2) ||
               (texCoords1 != nullptr && texCoords1->type != cgltf_type_vec2) ||
               (colors != nullptr && colors->type != cgltf_type_vec3 &&
                colors->type != cgltf_type_vec4))
            {
                throw std::runtime_error("GLB vertex attribute has an invalid element type.");
            }
            for(const cgltf_accessor* attribute : {normals, tangents, texCoords, texCoords1, colors})
            {
                if(attribute != nullptr && attribute->count != positions->count)
                {
                    throw std::runtime_error("GLB vertex attribute counts do not match POSITION.");
                }
            }
            const std::vector<float> normalValues = ReadAccessorFloats(normals);
            const std::vector<float> tangentValues = ReadAccessorFloats(tangents);
            const std::vector<float> texCoordValues = ReadAccessorFloats(texCoords);
            const std::vector<float> texCoord1Values = ReadAccessorFloats(texCoords1);
            const std::vector<float> colorValues = ReadAccessorFloats(colors);
            const std::size_t colorComponents = colors != nullptr
                ? cgltf_num_components(colors->type)
                : 0;

            mesh.Vertices.reserve(mesh.Vertices.size() + vertexCount);
            for(std::uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
            {
                TrSceneVertex vertex;
                vertex.Position =
                {
                    positionValues[vertexIndex * 3 + 0],
                    positionValues[vertexIndex * 3 + 1],
                    -positionValues[vertexIndex * 3 + 2]
                };
                if(normals != nullptr)
                {
                    vertex.Normal =
                    {
                        normalValues[vertexIndex * 3 + 0],
                        normalValues[vertexIndex * 3 + 1],
                        -normalValues[vertexIndex * 3 + 2]
                    };
                    Normalize(vertex.Normal);
                }
                else
                {
                    vertex.Normal = {};
                }
                if(tangents != nullptr)
                {
                    vertex.Tangent =
                    {
                        tangentValues[vertexIndex * 4 + 0],
                        tangentValues[vertexIndex * 4 + 1],
                        -tangentValues[vertexIndex * 4 + 2],
                        -tangentValues[vertexIndex * 4 + 3]
                    };
                }
                if(texCoords != nullptr)
                {
                    vertex.TexCoord0 =
                    {
                        texCoordValues[vertexIndex * 2 + 0],
                        texCoordValues[vertexIndex * 2 + 1]
                    };
                }
                if(texCoords1 != nullptr)
                {
                    vertex.TexCoord1 =
                    {
                        texCoord1Values[vertexIndex * 2 + 0],
                        texCoord1Values[vertexIndex * 2 + 1]
                    };
                }
                if(colors != nullptr && colorComponents >= 3)
                {
                    vertex.Color =
                    {
                        colorValues[vertexIndex * colorComponents + 0],
                        colorValues[vertexIndex * colorComponents + 1],
                        colorValues[vertexIndex * colorComponents + 2],
                        colorComponents >= 4 ? colorValues[vertexIndex * colorComponents + 3] : 1.0f
                    };
                }
                mesh.Vertices.push_back(vertex);
            }

            std::vector<std::uint32_t> localIndices = ReadIndices(source.indices, vertexCount);
            if(localIndices.size() % 3 != 0)
            {
                throw std::runtime_error("GLB triangle primitive index count is not divisible by three.");
            }
            for(std::size_t triangle = 0; triangle < localIndices.size(); triangle += 3)
            {
                std::swap(localIndices[triangle + 1], localIndices[triangle + 2]);
            }
            if(normals == nullptr)
            {
                GenerateNormals(mesh.Vertices, firstVertex, localIndices);
            }

            if(mesh.Indices.size() + localIndices.size() > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::overflow_error("GLB mesh exceeds 32-bit index addressing.");
            }
            TrScenePrimitive primitive;
            primitive.FirstVertex = firstVertex;
            primitive.VertexCount = vertexCount;
            primitive.FirstIndex = static_cast<std::uint32_t>(mesh.Indices.size());
            primitive.IndexCount = static_cast<std::uint32_t>(localIndices.size());
            primitive.MaterialIndex = IndexOf(
                source.material,
                data->materials,
                data->materials_count);
            for(std::uint32_t index : localIndices)
            {
                mesh.Indices.push_back(firstVertex + index);
            }
            mesh.Primitives.push_back(primitive);
            if(source.targets_count != 0)
            {
                result.Warnings.push_back(
                    "Morph targets on mesh '" + mesh.Name + "' were ignored.");
            }
        }
        scene.Meshes.push_back(std::move(mesh));
    }

    scene.Lights.reserve(data->lights_count);
    for(cgltf_size lightIndex = 0; lightIndex < data->lights_count; ++lightIndex)
    {
        const cgltf_light& source = data->lights[lightIndex];
        TrSceneLight light;
        light.Name = NameOrFallback(source.name, "Light_", lightIndex);
        switch(source.type)
        {
            case cgltf_light_type_directional: light.Type = TrSceneLightType::Directional; break;
            case cgltf_light_type_spot: light.Type = TrSceneLightType::Spot; break;
            default: light.Type = TrSceneLightType::Point; break;
        }
        std::copy_n(source.color, 3, light.Color.begin());
        light.Intensity = source.intensity;
        light.Range = source.range;
        light.InnerConeAngle = source.spot_inner_cone_angle;
        light.OuterConeAngle = source.spot_outer_cone_angle;
        scene.Lights.push_back(std::move(light));
    }

    scene.Cameras.reserve(data->cameras_count);
    for(cgltf_size cameraIndex = 0; cameraIndex < data->cameras_count; ++cameraIndex)
    {
        const cgltf_camera& source = data->cameras[cameraIndex];
        TrSceneCamera camera;
        camera.Name = NameOrFallback(source.name, "Camera_", cameraIndex);
        if(source.type == cgltf_camera_type_orthographic)
        {
            camera.Type = TrSceneCameraType::Orthographic;
            camera.HorizontalMagnification = source.data.orthographic.xmag;
            camera.VerticalMagnification = source.data.orthographic.ymag;
            camera.NearPlane = source.data.orthographic.znear;
            camera.FarPlane = source.data.orthographic.zfar;
        }
        else
        {
            camera.Type = TrSceneCameraType::Perspective;
            camera.AspectRatio = source.data.perspective.has_aspect_ratio
                ? source.data.perspective.aspect_ratio
                : 0.0f;
            camera.VerticalFieldOfView = source.data.perspective.yfov;
            camera.NearPlane = source.data.perspective.znear;
            camera.FarPlane = source.data.perspective.has_zfar
                ? source.data.perspective.zfar
                : 0.0f;
        }
        scene.Cameras.push_back(std::move(camera));
    }

    scene.Nodes.reserve(data->nodes_count);
    for(cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex)
    {
        const cgltf_node& source = data->nodes[nodeIndex];
        TrSceneNode node;
        node.Name = NameOrFallback(source.name, "Node_", nodeIndex);
        node.ParentIndex = IndexOf(source.parent, data->nodes, data->nodes_count);
        node.MeshIndex = IndexOf(source.mesh, data->meshes, data->meshes_count);
        node.LightIndex = IndexOf(source.light, data->lights, data->lights_count);
        node.CameraIndex = IndexOf(source.camera, data->cameras, data->cameras_count);
        node.Children.reserve(source.children_count);
        for(cgltf_size childIndex = 0; childIndex < source.children_count; ++childIndex)
        {
            node.Children.push_back(IndexOf(
                source.children[childIndex],
                data->nodes,
                data->nodes_count));
        }
        float localTransform[16] = {};
        float worldTransform[16] = {};
        cgltf_node_transform_local(&source, localTransform);
        cgltf_node_transform_world(&source, worldTransform);
        node.LocalTransform = ConvertMatrix(localTransform);
        node.WorldTransform = ConvertMatrix(worldTransform);
        if(source.skin != nullptr)
        {
            result.Warnings.push_back(
                "Skin on node '" + node.Name + "' was ignored; only static scenes are supported.");
        }
        if(source.has_mesh_gpu_instancing)
        {
            result.Warnings.push_back(
                "GPU instancing extension on node '" + node.Name + "' was ignored.");
        }
        scene.Nodes.push_back(std::move(node));
    }

    const cgltf_scene* defaultScene = data->scene;
    if(defaultScene == nullptr && data->scenes_count != 0)
    {
        defaultScene = &data->scenes[0];
    }
    if(defaultScene != nullptr)
    {
        scene.RootNodes.reserve(defaultScene->nodes_count);
        for(cgltf_size rootIndex = 0; rootIndex < defaultScene->nodes_count; ++rootIndex)
        {
            scene.RootNodes.push_back(IndexOf(
                defaultScene->nodes[rootIndex],
                data->nodes,
                data->nodes_count));
        }
    }
    else
    {
        for(std::uint32_t nodeIndex = 0; nodeIndex < scene.Nodes.size(); ++nodeIndex)
        {
            if(scene.Nodes[nodeIndex].ParentIndex == TrInvalidSceneIndex)
            {
                scene.RootNodes.push_back(nodeIndex);
            }
        }
    }

    if(data->animations_count != 0)
    {
        result.Warnings.push_back("Animations were ignored; this importer targets static scenes.");
    }
    if(data->skins_count != 0)
    {
        result.Warnings.push_back("Skins were ignored; this importer targets static scenes.");
    }

    scene.Validate();
    return result;
}
