#include "TrVulkanObject.h"

#include <glm/gtc/quaternion.hpp>

TrActor::TrActor()
{
    mTransform = glm::mat4(1.0f);
}

glm::vec3 TrActor::GetActorPosition()
{
    return glm::vec3(mTransform[3].x, mTransform[3].y, mTransform[3].z);
}

glm::vec3 TrActor::GetActorRotation()
{
    glm::quat quaternion = glm::quat_cast(mTransform);
    glm::vec3 eulerAngles = glm::eulerAngles(quaternion);
    return eulerAngles;
}

glm::vec3 TrActor::GetActorScale()
{
    return glm::vec3(mTransform[0].x, mTransform[1].y, mTransform[2].z);
}
