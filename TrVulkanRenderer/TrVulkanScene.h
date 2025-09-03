#pragma once
#include <vector>
#include "TrVulkanObject.h"

class TrScene
{
public:
    void AddActors(std::vector<TrActor> actors);
public:
    std::vector<TrActor> mSceneActors;
};
