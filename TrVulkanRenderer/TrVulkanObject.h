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
    TrActor(std::string modelReferencePath, std::shared_ptr<TrScene> ownerScene, glm::mat4 transform = glm::mat4(1.0f));

    glm::vec3 GetActorPosition();
    glm::vec3 GetActorRotation();
    glm::vec3 GetActorScale();
public:
    glm::mat4 mTransform;

    std::string mModelReferencePath;

    uint32_t mId;

    std::shared_ptr<TrScene> mOwnerScene;

#pragma region Render
    
    std::shared_ptr<TrObjModelRtBase> mObjModelRtData;

    std::shared_ptr<TrObjDescRtBase> mObjDescRtData;

    std::shared_ptr<TrObjModelRasterBase> mObjModelRasterData;

    std::shared_ptr<TrObjDescRasterBase> mObjDescRasterData;

#pragma endregion 
};