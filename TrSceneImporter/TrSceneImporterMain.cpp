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
        const TrSceneRenderMesh preview = verifiedScene.BuildStaticRenderMesh();
        std::uint64_t embeddedImageBytes = 0;
        for(const TrSceneImage& image : verifiedScene.Images)
        {
            embeddedImageBytes += image.Data.size();
        }

        std::ostringstream summary;
        summary
            << "Converted GLB to Tr Scene\n"
            << "  Scene: " << verifiedScene.Name << '\n'
            << "  Meshes: " << verifiedScene.Meshes.size() << '\n'
            << "  Nodes: " << verifiedScene.Nodes.size() << '\n'
            << "  Materials: " << verifiedScene.Materials.size() << '\n'
            << "  Textures: " << verifiedScene.Textures.size() << '\n'
            << "  Images: " << verifiedScene.Images.size() << '\n'
            << "  Embedded image bytes: " << embeddedImageBytes << '\n'
            << "  Lights: " << verifiedScene.Lights.size() << '\n'
            << "  Cameras: " << verifiedScene.Cameras.size() << '\n'
            << "  Preview vertices: " << preview.Vertices.size() << '\n'
            << "  Preview indices: " << preview.Indices.size() << '\n'
            << "  Preview bounds: ("
            << preview.BoundsCenter[0] << ", "
            << preview.BoundsCenter[1] << ", "
            << preview.BoundsCenter[2] << "), radius "
            << preview.BoundsRadius << '\n'
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
