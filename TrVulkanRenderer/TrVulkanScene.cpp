#include "TrVulkanScene.h"

void TrScene::AddActors(std::vector<TrActor> actors)
{
    for(auto& actor : actors)
    {
        mSceneActors.push_back(actor);
    }
}
