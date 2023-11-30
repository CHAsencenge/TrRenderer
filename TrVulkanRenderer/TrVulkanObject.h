#pragma once
#include <string>
#include <glm/glm.hpp>

#include "TrVulkanModel.h"
#include "TrVulkanShader.h"

class TrScene;

// can be added to a scene
class TrActor
{
public:
    TrActor(std::string modelReferencePath, TrScene* ownerScene, glm::mat4 transform = glm::mat4(1.0f));

    glm::vec3 GetActorPosition();
    glm::vec3 GetActorRotation();
    glm::vec3 GetActorScale();
public:
    glm::mat4 mTransform;

    std::string mModelReferencePath;

    uint32_t mId;

    TrScene* mOwnerScene;

#pragma region Render
    
    TrObjModelRtBase* mObjModelRtData;

    TrObjDescRtBase* mObjDescRtData;

    TrObjModelRasterBase* mObjModelRasterData;

    TrObjDescRasterBase* mObjDescRasterData;

#pragma endregion 
};