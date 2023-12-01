#include "TrVulkanScene.h"

void TrScene::AddActors(std::vector<std::shared_ptr<TrActor>>& actors)
{
    for(auto& actor : actors)
    {
        mSceneActors.insert({actor->mId, actor.get()});
    }
}
