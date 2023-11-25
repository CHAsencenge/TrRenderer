#pragma once
#include <string>
#include <glm/glm.hpp>

// can be added to a scene
class TrActor
{
public:
    TrActor();

    glm::vec3 GetActorPosition();
    glm::vec3 GetActorRotation();
    glm::vec3 GetActorScale();
public:
    glm::mat4 mTransform;

    std::string mModelReferencePath;
};