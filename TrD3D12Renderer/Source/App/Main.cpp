#include "TrWindowApp.h"

#include "Renderer/TrDeferredRenderer.h"
#include "TrLog.h"

#include <cstdlib>
#include <exception>
#include <optional>

_Use_decl_annotations_
int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR commandLine,
    int showCommand)
{
    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(commandLine);

    try
    {
        TrLog::InitializeForApplication("TrD3D12Renderer");
        TrLog::Info("Application starting.");
        std::optional<std::wstring> sceneOverride;
        int exitCode = EXIT_SUCCESS;
        do
        {
            TrDeferredRenderer renderer(1920, 1080, L"Tr Cornell Box");
            exitCode = TrWindowApp::Run(
                &renderer,
                instance,
                showCommand,
                sceneOverride);
            sceneOverride = renderer.GetRequestedScenePath();
        }
        while(sceneOverride.has_value());
        TrLog::Info("Application stopped normally.");
        TrLog::Shutdown();
        return exitCode;
    }
    catch(const TrGraphicsException& exception)
    {
        const std::wstring message = exception.ToString();
        TrLog::Error(message);
        MessageBoxW(
            nullptr,
            message.c_str(),
            L"Direct3D 12 Error",
            MB_OK | MB_ICONERROR);
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
    catch(const std::exception& exception)
    {
        TrLog::Error(exception.what());
        MessageBoxA(
            nullptr,
            exception.what(),
            "Tr Renderer Error",
            MB_OK | MB_ICONERROR);
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
    catch(...)
    {
        TrLog::Error("Unknown fatal exception.");
        MessageBoxA(
            nullptr,
            "Unknown fatal exception.",
            "Tr Renderer Error",
            MB_OK | MB_ICONERROR);
        TrLog::Shutdown();
        return EXIT_FAILURE;
    }
}
