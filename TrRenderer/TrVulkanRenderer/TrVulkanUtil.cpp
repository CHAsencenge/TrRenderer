#include "TrVulkanUtil.h"

std::vector<char> TrVulkanUtil::ReadFile(const std::string& filename)
{
    // ate: read from file end, can determine the file size
    // filename: relative to "working directory"
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open())
    {
        throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::OPEN_FILE_FAILED]);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    // skip from file end to file begin
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::string TrVulkanUtil::FilenameToModelFilename(const std::string& filename)
{
    return "Models/" + filename + ".obj"; 
}

std::string TrVulkanUtil::FilenameToTexFilename(const std::string& filename, const std::string& suffix)
{
    return "Textures/" + filename + suffix; 
}
