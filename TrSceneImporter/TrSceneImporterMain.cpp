#include "TrGlbImporter.h"
#include "TrLog.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    int RunImporter(const std::filesystem::path& input, std::filesystem::path output)
    {
        TrLog::Info("Importing GLB: " + input.u8string());
        if(output.empty())
        {
            output = input;
            output.replace_extension(".trscene");
        }

        TrGlbImportResult imported = TrGlbImporter::Import(input);
        imported.Scene.Save(output);

        // Read the generated file back immediately so format/version mistakes
        // fail during conversion instead of later in the renderer.
        const TrScene verifiedScene = TrScene::Load(output);
        const std::vector<std::uint32_t> activeNodes =
            verifiedScene.GetActiveNodeIndices();
        const TrSceneBounds worldBounds = verifiedScene.CalculateWorldBounds();
        std::uint64_t embeddedImageBytes = 0;
        for(const TrSceneImage& image : verifiedScene.Images)
        {
            embeddedImageBytes += image.Data.size();
        }
        std::uint64_t vertexCount = 0;
        std::uint64_t indexCount = 0;
        std::uint64_t primitiveCount = 0;
        for(const TrSceneMesh& mesh : verifiedScene.Meshes)
        {
            vertexCount += mesh.Vertices.size();
            indexCount += mesh.Indices.size();
            primitiveCount += mesh.Primitives.size();
        }
        std::uint64_t meshInstanceCount = 0;
        for(const std::uint32_t nodeIndex : activeNodes)
        {
            if(verifiedScene.Nodes[nodeIndex].MeshIndex != TrInvalidSceneIndex)
            {
                ++meshInstanceCount;
            }
        }

        std::ostringstream summary;
        summary
            << "Converted GLB to Tr Scene\n"
            << "  Scene: " << verifiedScene.Name << '\n'
            << "  Meshes: " << verifiedScene.Meshes.size() << '\n'
            << "  Nodes: " << verifiedScene.Nodes.size() << '\n'
            << "  Active nodes: " << activeNodes.size() << '\n'
            << "  Mesh instances: " << meshInstanceCount << '\n'
            << "  Materials: " << verifiedScene.Materials.size() << '\n'
            << "  Textures: " << verifiedScene.Textures.size() << '\n'
            << "  Images: " << verifiedScene.Images.size() << '\n'
            << "  Embedded image bytes: " << embeddedImageBytes << '\n'
            << "  Lights: " << verifiedScene.Lights.size() << '\n'
            << "  Cameras: " << verifiedScene.Cameras.size() << '\n'
            << "  Geometry vertices: " << vertexCount << '\n'
            << "  Geometry indices: " << indexCount << '\n'
            << "  Primitives: " << primitiveCount << '\n'
            << "  World AABB: min ("
            << worldBounds.Minimum[0] << ", "
            << worldBounds.Minimum[1] << ", "
            << worldBounds.Minimum[2] << "), max ("
            << worldBounds.Maximum[0] << ", "
            << worldBounds.Maximum[1] << ", "
            << worldBounds.Maximum[2] << ")\n"
            << "  Bounding sphere: center ("
            << worldBounds.BoundsCenter[0] << ", "
            << worldBounds.BoundsCenter[1] << ", "
            << worldBounds.BoundsCenter[2] << "), radius "
            << worldBounds.BoundsRadius << '\n'
            << "  Output: " << output.u8string();
        TrLog::Info(summary.str());
        for(const std::string& warning : imported.Warnings)
        {
            TrLog::Warn(warning);
        }
        return EXIT_SUCCESS;
    }

    int ReportUsage()
    {
        TrLog::Error("Usage: TrSceneImporter <input.glb> [output.trscene]");
        return EXIT_FAILURE;
    }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
{
    try
    {
        TrLog::InitializeForApplication("TrSceneImporter", true);
        if(argc < 2 || argc > 3)
        {
            const int exitCode = ReportUsage();
            TrLog::Shutdown();
            return exitCode;
        }
        const int exitCode = RunImporter(
            argv[1],
            argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::path());
        TrLog::Shutdown();
        return exitCode;
    }
    catch(const std::exception& exception)
    {
        TrLog::Error(std::string("Import failed: ") + exception.what());
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
    catch(...)
    {
        TrLog::Error("Import failed with an unknown exception.");
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
}
#else
int main(int argc, char* argv[])
{
    try
    {
        TrLog::InitializeForApplication("TrSceneImporter", true);
        if(argc < 2 || argc > 3)
        {
            const int exitCode = ReportUsage();
            TrLog::Shutdown();
            return exitCode;
        }
        const int exitCode = RunImporter(
            argv[1],
            argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::path());
        TrLog::Shutdown();
        return exitCode;
    }
    catch(const std::exception& exception)
    {
        TrLog::Error(std::string("Import failed: ") + exception.what());
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
    catch(...)
    {
        TrLog::Error("Import failed with an unknown exception.");
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
}
#endif
