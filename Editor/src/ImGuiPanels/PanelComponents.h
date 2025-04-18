#pragma once

#include "imgui.h"

#include <cstdint>
#include <vector>
#include <string>
#include <utility> // For std::pair

// Structure to hold data for texture display
struct TextureDisplayData {
    uint64_t textureID;
    std::string label;
    float width = 0.0f;  // Optional: Provide for aspect ratio calculation
    float height = 0.0f; // Optional: Provide for aspect ratio calculation
    // Invert default UVs for OpenGL texture orientation
    ImVec2 uv0 = ImVec2(0, 1); // Top-left UV coordinate
    ImVec2 uv1 = ImVec2(1, 0); // Bottom-right UV coordinate
};

// Function to draw a grid of textures
void drawTextureGrid(
    const std::vector<TextureDisplayData>& textures,
    int columns = 1,          // Max columns per row
    float maxItemWidth = 150.0f, // Max width for each texture item
    const ImVec4& tint_col = ImVec4(1, 1, 1, 1),   // Tint color for ImGui::Image
    const ImVec4& border_col = ImVec4(0, 0, 0, 0) // Border color for ImGui::Image
);







