#pragma once
#include <vector>
#include "TrVulkanObject.h"

class TrScene
{
public:
    void AddActors(std::vector<TrActor> actors);

    uint32_t AllocActorId() { return mActorsIdAllocator++; }
public:
    std::vector<TrActor*> mSceneActors;

    uint32_t mActorsIdAllocator {0};
};
