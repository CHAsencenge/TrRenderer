#include "TrLog.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <codecvt>
#include <locale>
#endif

namespace
{
    struct TrLogState
    {
        std::mutex Mutex;
        std::ofstream Stream;
        std::filesystem::path FilePath;
        bool MirrorToConsole = false;
        bool Initialized = false;
    };

    TrLogState& GetState()
    {
        static TrLogState state;
        return state;
    }

    const char* LevelName(TrLogLevel level)
    {
        switch(level)
        {
            case TrLogLevel::Debug: return "DEBUG";
            case TrLogLevel::Info: return "INFO";
            case TrLogLevel::Warning: return "WARN";
            case TrLogLevel::Error: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    std::string CreateTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime = {};
#if defined(_WIN32)
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif
        std::ostringstream stream;
        stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
               << '.' << std::setfill('0') << std::setw(3)
               << milliseconds.count();
        return stream.str();
    }

    std::string ToUtf8(std::wstring_view value)
    {
        if(value.empty())
        {
            return {};
        }
#if defined(_WIN32)
        if(value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return "<wide log message is too large>";
        }
        const int sourceLength = static_cast<int>(value.size());
        const int requiredSize = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            sourceLength,
            nullptr,
            0,
            nullptr,
            nullptr);
        if(requiredSize <= 0)
        {
            return "<failed to convert wide log message to UTF-8>";
        }
        std::string result(static_cast<std::size_t>(requiredSize), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            sourceLength,
            result.data(),
            requiredSize,
            nullptr,
            nullptr);
        return result;
#else
        return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(
            value.data(),
            value.data() + value.size());
#endif
    }

    std::filesystem::path GetExecutableDirectory()
    {
#if defined(_WIN32)
        std::vector<wchar_t> pathBuffer(32768);
        const DWORD length = GetModuleFileNameW(
            nullptr,
            pathBuffer.data(),
            static_cast<DWORD>(pathBuffer.size()));
        if(length != 0 && length < pathBuffer.size())
        {
            return std::filesystem::path(
                std::wstring(pathBuffer.data(), length)).parent_path();
        }
#endif
        return std::filesystem::current_path();
    }

    void WriteOutput(TrLogState& state, TrLogLevel level, const std::string& line)
    {
        if(state.Stream.is_open())
        {
            state.Stream << line << '\n';
            state.Stream.flush();
        }

#if defined(_WIN32)
        const std::string debugLine = line + '\n';
        OutputDebugStringA(debugLine.c_str());
#else
        if(!state.MirrorToConsole)
        {
            std::clog << line << '\n';
        }
#endif

        if(state.MirrorToConsole)
        {
            std::ostream& output = level == TrLogLevel::Warning || level == TrLogLevel::Error
                ? std::cerr
                : std::cout;
            output << line << '\n';
            output.flush();
        }
    }
}

void TrLog::InitializeForApplication(
    std::string_view applicationName,
    bool mirrorToConsole)
{
    if(applicationName.empty())
    {
        throw std::invalid_argument("Log application name cannot be empty.");
    }
    const std::filesystem::path logDirectory = GetExecutableDirectory() / "logs";
    Initialize(
        logDirectory / (std::string(applicationName) + ".log"),
        mirrorToConsole);
}

void TrLog::Initialize(
    const std::filesystem::path& filePath,
    bool mirrorToConsole)
{
    if(filePath.empty())
    {
        throw std::invalid_argument("Log file path cannot be empty.");
    }

    TrLogState& state = GetState();
    {
        std::lock_guard<std::mutex> lock(state.Mutex);
        state.MirrorToConsole = mirrorToConsole;
    }

    const std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
    const std::filesystem::path parentPath = absolutePath.parent_path();
    if(!parentPath.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(parentPath, error);
        if(error)
        {
            throw std::runtime_error(
                "Failed to create log directory: " + error.message());
        }
    }

    {
        std::lock_guard<std::mutex> lock(state.Mutex);
        if(state.Stream.is_open())
        {
            state.Stream.close();
        }
        state.Stream.clear();
        state.Stream.open(absolutePath, std::ios::binary | std::ios::out | std::ios::trunc);
        if(!state.Stream)
        {
            throw std::runtime_error("Failed to open log file for writing.");
        }
        state.FilePath = absolutePath;
        state.MirrorToConsole = mirrorToConsole;
        state.Initialized = true;
    }

    Info("Log initialized: " + absolutePath.u8string());
}

void TrLog::Shutdown()
{
    TrLogState& state = GetState();
    std::lock_guard<std::mutex> lock(state.Mutex);
    if(state.Stream.is_open())
    {
        state.Stream.flush();
        state.Stream.close();
    }
    state.Initialized = false;
    state.MirrorToConsole = false;
}

bool TrLog::IsInitialized()
{
    TrLogState& state = GetState();
    std::lock_guard<std::mutex> lock(state.Mutex);
    return state.Initialized;
}

std::filesystem::path TrLog::GetFilePath()
{
    TrLogState& state = GetState();
    std::lock_guard<std::mutex> lock(state.Mutex);
    return state.FilePath;
}

void TrLog::Write(TrLogLevel level, std::string_view message)
{
    TrLogState& state = GetState();
    std::lock_guard<std::mutex> lock(state.Mutex);

    std::size_t lineStart = 0;
    do
    {
        const std::size_t lineEnd = message.find('\n', lineStart);
        const std::size_t characterCount = lineEnd == std::string_view::npos
            ? message.size() - lineStart
            : lineEnd - lineStart;
        std::string line(message.substr(lineStart, characterCount));
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const std::string formatted =
            "[" + CreateTimestamp() + "] [" + LevelName(level) + "] " + line;
        WriteOutput(state, level, formatted);

        if(lineEnd == std::string_view::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
    while(lineStart < message.size());
}

void TrLog::Write(TrLogLevel level, std::wstring_view message)
{
    Write(level, ToUtf8(message));
}
