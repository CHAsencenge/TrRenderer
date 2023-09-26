#include "TrVulkanImGuiConfig.h"

#include <iostream>

#include "TrVulkanGlobalConfigs.h"


void TrVulkanImGuiConfig::ShowTrVulkanConfig(bool* bOpen)
{
    IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing Dear ImGui context.");
    
    if(TrVulkanGlobal::bTestBool)
    {
        std::cout << "TrVulkanGlobal::bTestBool true" << std::endl;
    }
    

    if (!ImGui::Begin("TrVulkan ImGui Config", bOpen))
    {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }

    ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

    if (ImGui::CollapsingHeader("Bool Settings"))
    {
        if (ImGui::BeginTable("split", 3))
        {
            ImGui::TableNextColumn(); ImGui::Checkbox("test bool", &TrVulkanGlobal::bTestBool);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("MVP Params"))
    {
        if (ImGui::TreeNode("MVP Params"))
        {
            ImGui::SeparatorText("Model");
            
            ImGui::InputFloat("rModelScaleRate", &TrVulkanGlobal::rModelScaleRate, 0.01f, 1.0f, "%.4f");
            static float drag_f = 0.5f;
            ImGui::DragFloat("modelScaleRate", &TrVulkanGlobal::modelScaleRate, TrVulkanGlobal::rModelScaleRate);
            // ImGui::DragFloat("modelScaleRate", &drag_f, 0.01f);
            
            ImGui::InputFloat("rModelAngleAxis", &TrVulkanGlobal::rModelAngleAxis, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("modelAngleAxisX", &TrVulkanGlobal::modelAngleAxis.x, TrVulkanGlobal::rModelScaleRate);
            ImGui::DragFloat("modelAngleAxisY", &TrVulkanGlobal::modelAngleAxis.y, TrVulkanGlobal::rModelScaleRate);
            ImGui::DragFloat("modelAngleAxisZ", &TrVulkanGlobal::modelAngleAxis.z, TrVulkanGlobal::rModelScaleRate);

            
            
            static ImGuiSliderFlags flags = ImGuiSliderFlags_None;
            ImGui::InputFloat("rModelRadiansAngle", &TrVulkanGlobal::rModelRadiansAngle, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("modelRadiansAngle", &TrVulkanGlobal::modelRadiansAngle, 0.05f);
            
            ImGui::SeparatorText("View");

            ImGui::InputFloat("rViewEye", &TrVulkanGlobal::rViewEye, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("viewEyeX", &TrVulkanGlobal::viewEye.x, TrVulkanGlobal::rViewEye);
            ImGui::DragFloat("viewEyeY", &TrVulkanGlobal::viewEye.y, TrVulkanGlobal::rViewEye);
            ImGui::DragFloat("viewEyeZ", &TrVulkanGlobal::viewEye.z, TrVulkanGlobal::rViewEye);

            ImGui::InputFloat("rViewCenter", &TrVulkanGlobal::rViewCenter, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("viewCenterX", &TrVulkanGlobal::viewCenter.x, TrVulkanGlobal::rViewCenter);
            ImGui::DragFloat("viewCenterY", &TrVulkanGlobal::viewCenter.y, TrVulkanGlobal::rViewCenter);
            ImGui::DragFloat("viewCenterZ", &TrVulkanGlobal::viewCenter.z, TrVulkanGlobal::rViewCenter);

            ImGui::InputFloat("rViewUp", &TrVulkanGlobal::rViewUp, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("viewUpX", &TrVulkanGlobal::viewUp.x, TrVulkanGlobal::rViewUp);
            ImGui::DragFloat("viewUpY", &TrVulkanGlobal::viewUp.y, TrVulkanGlobal::rViewUp);
            ImGui::DragFloat("viewUpZ", &TrVulkanGlobal::viewUp.z, TrVulkanGlobal::rViewUp);
            
            ImGui::SeparatorText("Proj");

            ImGui::InputFloat("rProjRadiansFovy", &TrVulkanGlobal::rProjRadiansFovy, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("projRadiansFovy", &TrVulkanGlobal::projRadiansFovy, TrVulkanGlobal::rProjRadiansFovy);

            ImGui::InputFloat("rProjZNear", &TrVulkanGlobal::rProjZNear, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("projZNear", &TrVulkanGlobal::projZNear, TrVulkanGlobal::rProjZNear);

            ImGui::InputFloat("rProjZFar", &TrVulkanGlobal::rProjZFar, 0.01f, 1.0f, "%.4f");
            ImGui::DragFloat("projZFar", &TrVulkanGlobal::projZFar, TrVulkanGlobal::rProjZFar);
            
            ImGui::TreePop();
            ImGui::Spacing();
        }
    }

    ImGui::PopItemWidth();
    ImGui::End();
}
