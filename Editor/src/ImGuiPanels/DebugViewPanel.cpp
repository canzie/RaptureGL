#include "DebugViewPanel.h"
#include "Renderer/Deferred Shading/DeferredRenderer.h"
#include "Logger/Log.h"

void DebugViewPanel::render()
{
    if (!m_enabled)
        return;

    ImGui::Begin("GBuffer Debug View");
    
    // Get the size of the ImGui window viewport
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    // Calculate texture display size (2 textures per row)
    const float padding = 10.0f;
    float textureWidth = (viewportPanelSize.x - padding) / 2.0f;
    float textureHeight = textureWidth * 0.5625f; // 16:9 aspect ratio
    ImVec2 textureSize(textureWidth, textureHeight);
    
    // Get GBuffer from the DeferredRenderer
    auto gBuffer = Rapture::DeferredRenderer::getGBuffer();
    if (!gBuffer)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "GBuffer not available");
        ImGui::End();
        return;
    }
    
    // Display Position texture
    try {
        displayTexture("Position", gBuffer->getPositionTextureID(), textureSize);
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to display Position texture: {0}", e.what());
    }
    
    // Display Normal texture (side by side)
    ImGui::SameLine();
    try {
        displayTexture("Normal", gBuffer->getNormalTextureID(), textureSize);
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to display Normal texture: {0}", e.what());
    }
    
    // Display Albedo texture (new row)
    try {
        displayTexture("Albedo", gBuffer->getAlbedoTextureID(), textureSize);
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to display Albedo texture: {0}", e.what());
    }
    
    // Display Material texture (side by side)
    ImGui::SameLine();
    try {
        displayTexture("Material", gBuffer->getMaterialTextureID(), textureSize);
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to display Material texture: {0}", e.what());
    }
    
    // Display Depth texture (new row)
    try {
        displayTexture("Depth", gBuffer->getDepthTextureID(), textureSize);
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to display Depth texture: {0}", e.what());
    }
    
    ImGui::End();
}

void DebugViewPanel::displayTexture(const char* label, uint32_t textureID, ImVec2 size)
{
    if (textureID == 0)
    {
        ImGui::BeginChild(label, size, true);
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Texture ID is invalid");
        ImGui::EndChild();
        return;
    }
    
    // Create a group with border for the texture
    ImGui::BeginChild(label, ImVec2(size.x, size.y + 20), true);
    
    // Display texture label
    ImGui::Text("%s", label);
    
    // Display the texture (ImGui::Image uses void* to store the texture ID)
    ImTextureID texID = (ImTextureID)(intptr_t)textureID;
    ImGui::Image(texID, size, ImVec2(0, 1), ImVec2(1, 0)); // Flip Y to match OpenGL coordinates
    
    ImGui::EndChild();
} 