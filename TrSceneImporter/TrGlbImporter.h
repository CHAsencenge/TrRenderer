#pragma once

#include "TrScene.h"

struct TrGlbImportResult
{
    TrScene Scene;
    std::vector<std::string> Warnings;
};

class TrGlbImporter
{
public:
    static TrGlbImportResult Import(const std::filesystem::path& path);
};

