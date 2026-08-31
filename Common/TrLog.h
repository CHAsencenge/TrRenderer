#pragma once

#include <filesystem>
#include <string_view>

enum class TrLogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

// Process-wide, thread-safe logger. Initialize opens the file with truncation,
// so every application launch starts with a clean log.
class TrLog
{
public:
    static void InitializeForApplication(
        std::string_view applicationName,
        bool mirrorToConsole = false);
    static void Initialize(
        const std::filesystem::path& filePath,
        bool mirrorToConsole = false);
    static void Shutdown();

    static bool IsInitialized();
    static std::filesystem::path GetFilePath();

    static void Write(TrLogLevel level, std::string_view message);
    static void Write(TrLogLevel level, std::wstring_view message);

    static void Debug(std::string_view message) { Write(TrLogLevel::Debug, message); }
    static void Info(std::string_view message) { Write(TrLogLevel::Info, message); }
    static void Warn(std::string_view message) { Write(TrLogLevel::Warning, message); }
    static void Error(std::string_view message) { Write(TrLogLevel::Error, message); }

    static void Debug(std::wstring_view message) { Write(TrLogLevel::Debug, message); }
    static void Info(std::wstring_view message) { Write(TrLogLevel::Info, message); }
    static void Warn(std::wstring_view message) { Write(TrLogLevel::Warning, message); }
    static void Error(std::wstring_view message) { Write(TrLogLevel::Error, message); }
};
