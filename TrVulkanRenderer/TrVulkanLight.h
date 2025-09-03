#pragma once
#include <glm/glm.hpp>
#include "TrVulkanObject.h"

class TrLightBase : public TrActor
{
public:
    TrLightBase(std::string modelReferencePath, glm::mat4 transform = glm::mat4(1.0f), glm::vec3 color = {1.0f, 1.0f, 1.0f});

protected:
    glm::vec3 mColor;
};


class TrLantern : public TrLightBase
{
public:
    TrLantern(std::string modelReferencePath, glm::mat4 transform = glm::mat4(1.0f), glm::vec3 color = {1.0f, 1.0f, 1.0f});

protected:
    float mRadius; // illuminates distance
    float mBrightness;
};