#pragma once
#include <glm/glm.hpp>
#include "TrVulkanObject.h"

class TrLightBase : public TrActor
{
public:
    TrLightBase();
public:
    glm::vec3 mColor;
};

class TrLantern : public TrLightBase
{
    float mRadius; // illuminates distance
    float mBrightness;
};