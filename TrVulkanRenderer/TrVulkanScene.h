#pragma once
#include <vector>
#include "TrVulkanObject.h"

class TrScene
{
public:
    void AddActors(std::vector<std::shared_ptr<TrActor>>& actors);

    uint32_t AllocActorId() { return mActorsIdAllocator++; }
public:
    std::map<uint32_t, TrActor*> mSceneActors;

    uint32_t mActorsIdAllocator = 0;
};
