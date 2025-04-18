#include "DebugViewPanel.h"
#include "Renderer/Deferred Shading/DeferredRenderer.h"
#include "Renderer/Deferred Shading/GBuffer.h" // Include GBuffer header for dimensions
#include "Logger/Log.h"
#include "PanelComponents.h" // Include the new PanelComponents header
#include <vector>

void DebugViewPanel::render()
{
    if (!m_enabled)
        return;

    ImGui::Begin("GBuffer Debug View");
    
    // Get GBuffer from the DeferredRenderer
    auto gBuffer = Rapture::DeferredRenderer::getGBuffer();
    if (!gBuffer)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "GBuffer not available");
        ImGui::End();
        return;
    }

    // Prepare texture data for the grid
    std::vector<TextureDisplayData> texturesToShow;
    float gBufferWidth = 0.0f;
    float gBufferHeight = 0.0f;

    try {
        // Assuming GBuffer class has methods to get dimensions
        gBufferWidth = static_cast<float>(gBuffer->getSpecification().width);
        gBufferHeight = static_cast<float>(gBuffer->getSpecification().height);
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to get GBuffer dimensions: {0}", e.what());
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Could not get GBuffer dimensions.");
        // Optionally provide default dimensions or return
    }

    // Define the UV coordinates for flipping (OpenGL)
    ImVec2 uv0_flipped = ImVec2(0, 1);
    ImVec2 uv1_flipped = ImVec2(1, 0);

    // Add GBuffer textures to the list
    try {
        texturesToShow.push_back({gBuffer->getPositionTextureID(), "Position", gBufferWidth, gBufferHeight, uv0_flipped, uv1_flipped});
        texturesToShow.push_back({gBuffer->getNormalTextureID(), "Normal", gBufferWidth, gBufferHeight, uv0_flipped, uv1_flipped});
        texturesToShow.push_back({gBuffer->getAlbedoTextureID(), "Albedo", gBufferWidth, gBufferHeight, uv0_flipped, uv1_flipped});
        texturesToShow.push_back({gBuffer->getMaterialTextureID(), "Material", gBufferWidth, gBufferHeight, uv0_flipped, uv1_flipped});
        texturesToShow.push_back({gBuffer->getDepthTextureID(), "Depth", gBufferWidth, gBufferHeight, uv0_flipped, uv1_flipped});
        // Add more textures here if needed (e.g., emissive, specular)
    } catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Failed to get one or more GBuffer texture IDs: {0}", e.what());
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error accessing GBuffer textures.");
    }

    // Calculate max width based on available space (aim for 2 columns)
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float padding = ImGui::GetStyle().ItemSpacing.x;
    float maxItemWidth = (availableWidth - padding) / 2.0f;

    // Draw the textures in a grid (2 columns)
    drawTextureGrid(texturesToShow, 2, maxItemWidth);

    ImGui::End();
}

