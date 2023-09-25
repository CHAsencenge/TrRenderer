#include "TrVulkanImGuiConfig.h"

#include <iostream>

#include "TrVulkanGlobalConfigs.h"


void TrVulkanImGuiConfig::ShowTrVulkanConfig(bool* bOpen)
{
    IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing Dear ImGui context.");

    static bool testBool = false;

    TrVulkanGlobal::bTestBool = testBool;
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

    /*if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Menu"))
        {
            ImGui::EndMenu();
        }
    }*/

    if (ImGui::CollapsingHeader("Bool Settings"))
    {
        if (ImGui::BeginTable("split", 3))
        {
            ImGui::TableNextColumn(); ImGui::Checkbox("test bool", &testBool);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("MVP Params"))
    {
        if (ImGui::BeginTable("split", 3))
        {
            ImGui::TableNextColumn(); ImGui::Checkbox("test bool", &testBool);

            ImGui::EndTable();
        }
    }

    ImGui::PopItemWidth();
    ImGui::End();
}
