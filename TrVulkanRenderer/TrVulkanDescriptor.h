#pragma once
#include "glm/glm.hpp"

// descriptor usage
// specify descriptor layout when creating pipeline
// allocate descriptor set from descriptor pool
// bind descriptor set when rendering

// uniform buffer object
struct TrVulkanTransformUBO
{
    // glm can match glm::mat4 and mat4 between C++ and glsl
    glm::mat4 mModel;
    glm::mat4 mView;
    glm::mat4 mProj;
};