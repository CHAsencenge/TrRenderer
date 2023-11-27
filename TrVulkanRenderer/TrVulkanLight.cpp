#include "TrVulkanLight.h"

TrLightBase::TrLightBase(std::string modelReferencePath, glm::mat4 transform, glm::vec3 color) : TrActor(modelReferencePath, transform)
{
    mColor = color;
}

TrLantern::TrLantern(std::string modelReferencePath, glm::mat4 transform, glm::vec3 color) : TrLightBase(modelReferencePath, transform, color)
{
    
}
