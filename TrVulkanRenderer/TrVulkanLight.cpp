#include "TrVulkanLight.h"

TrLightBase::TrLightBase(std::string modelReferencePath, std::shared_ptr<TrScene> ownerScene, glm::mat4 transform, glm::vec3 color) : TrActor(modelReferencePath, ownerScene, transform)
{
    mColor = color;
}

TrLantern::TrLantern(std::string modelReferencePath, std::shared_ptr<TrScene> ownerScene, glm::mat4 transform, glm::vec3 color) : TrLightBase(modelReferencePath, ownerScene, transform, color)
{
    
}
