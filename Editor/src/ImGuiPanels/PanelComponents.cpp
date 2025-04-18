#include "PanelComponents.h"
#include <algorithm> // For std::min/max
#include <vector>

// Implementation of drawTextureGrid
void drawTextureGrid(
    const std::vector<TextureDisplayData>& textures,
    int columns,
    float maxItemWidth,
    const ImVec4& tint_col,
    const ImVec4& border_col)
{
    if (textures.empty() || columns <= 0) {
        return;
    }

    // Ensure columns is at least 1
    columns = std::max(1, columns);

    float panelWidth = ImGui::GetContentRegionAvail().x;
    float itemSpacing = ImGui::GetStyle().ItemSpacing.x;

    // Calculate the width for each item based on columns, panel width, and spacing
    float effectiveWidth = panelWidth - (columns - 1) * itemSpacing;
    float calculatedItemWidth = effectiveWidth / columns;
    float actualItemWidth = std::min(maxItemWidth, calculatedItemWidth);

    int currentColumn = 0;
    for (size_t i = 0; i < textures.size(); ++i) {
        const auto& texData = textures[i];

        // Handle potential invalid texture ID
        if (texData.textureID == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "Invalid Tex ID for '%s'", texData.label.c_str());
            continue; // Skip this texture
        }

        // Add spacing if not the first item in the row
        if (currentColumn > 0) {
            ImGui::SameLine();
        }

        ImGui::BeginGroup();

        if (!texData.label.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(texData.label.c_str());
            float textPosX = (actualItemWidth - textSize.x) * 0.5f;
            if (textPosX > 0.0f) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textPosX);
            }
            ImGui::TextUnformatted(texData.label.c_str());
        }

        // Calculate preview dimensions maintaining aspect ratio if possible
        ImVec2 previewDimensions(actualItemWidth, actualItemWidth);
        if (texData.width > 0 && texData.height > 0) {
            float aspectRatio = texData.width / texData.height;
            if (aspectRatio > 1.0f) {
                previewDimensions = ImVec2(actualItemWidth, actualItemWidth / aspectRatio);
            } else {
                previewDimensions = ImVec2(actualItemWidth * aspectRatio, actualItemWidth);
            }
        }

        ImGui::Image((ImTextureID)texData.textureID,
                     previewDimensions,
                     texData.uv0,
                     texData.uv1,
                     tint_col,
                     border_col);



        ImGui::EndGroup();

        // Move to the next column/row
        currentColumn++;
        if (currentColumn >= columns) {
            currentColumn = 0;
            // No ImGui::NewLine() needed, ImGui handles wrapping with SameLine
        }
    }
}

