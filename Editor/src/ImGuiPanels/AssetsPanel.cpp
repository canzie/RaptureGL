#include "AssetsPanel.h"
#include "Logger/Log.h"
#include "AssetsManager/AssetManager.h"
#include "Materials/MaterialLibrary.h"
#include "Textures/Texture.h"

#include <algorithm>
#include <filesystem>
#include <utility>
#include <sstream>

namespace fs = std::filesystem;

AssetsPanel::AssetsPanel() {
}

void AssetsPanel::setRootDirectory(const std::string& rootDir) {
    m_rootDirectory = rootDir;
    m_currentDirectory = rootDir;
    scanCurrentDirectory();
}

void AssetsPanel::scanCurrentDirectory() {
    m_fileItems.clear();
    
    try {
        // Check if directory exists
        if (!fs::exists(m_currentDirectory) || !fs::is_directory(m_currentDirectory)) {
            Rapture::GE_INFO("Invalid directory: {0}", m_currentDirectory);
            // Fallback to root directory
            if (fs::exists(m_rootDirectory) && fs::is_directory(m_rootDirectory)) {
                m_currentDirectory = m_rootDirectory;
            } else {
                Rapture::GE_INFO("Root directory also invalid: {0}", m_rootDirectory);
                return;
            }
        }
        
        // Check path length to avoid issues
        if (m_currentDirectory.length() > 240) { // Windows MAX_PATH is 260, leaving some room
            Rapture::GE_INFO("Path too long, may cause issues: {0}", m_currentDirectory);
            // Consider warning the user here
        }

        // Scan directories first, then files
        std::vector<FileItem> directories;
        std::vector<FileItem> files;
        
        // Use a safety counter to limit entries in case of very large directories
        const size_t MAX_ENTRIES = 500;
        size_t entryCount = 0;
        
        fs::directory_iterator dirIt;
        try {
            dirIt = fs::directory_iterator(m_currentDirectory);
        } catch (const std::exception& e) {
            Rapture::GE_INFO("Failed to create directory iterator: {0}", e.what());
            return;
        }
        
        for (const auto& entry : dirIt) {
            if (entryCount >= MAX_ENTRIES) {
                Rapture::GE_INFO("Too many files in directory, limiting display to {0} items", MAX_ENTRIES);
                break;
            }
            
            try {
                FileItem item;
                item.name = entry.path().filename().string();
                
                // Skip files/folders starting with a period (hidden)
                if (!item.name.empty() && item.name[0] == '.') {
                    continue;
                }
                
                // Check if the name contains non-printable characters
                bool hasNonPrintable = false;
                for (char c : item.name) {
                    if (c < 32 || c > 126) {
                        hasNonPrintable = true;
                        break;
                    }
                }
                
                if (hasNonPrintable) {
                    continue; // Skip files with non-printable characters
                }
                
                item.path = entry.path().string();
                
                // Check for directory status safely
                try {
                    item.isDirectory = entry.is_directory();
                } catch (...) {
                    // If we can't determine if it's a directory, skip it
                    continue;
                }
                
                if (item.isDirectory) {
                    directories.push_back(item);
                } else {
                    files.push_back(item);
                }
                
                entryCount++;
            } catch (const std::exception& e) {
                Rapture::GE_INFO("Error processing entry: {0}", e.what());
                // Continue to next entry
            }
        }
        
        // Sort alphabetically
        auto sortFunc = [](const FileItem& a, const FileItem& b) {
            return a.name < b.name;
        };
        
        std::sort(directories.begin(), directories.end(), sortFunc);
        std::sort(files.begin(), files.end(), sortFunc);
        
        // Add directories first, then files
        m_fileItems.insert(m_fileItems.end(), directories.begin(), directories.end());
        m_fileItems.insert(m_fileItems.end(), files.begin(), files.end());
        
    } catch (const std::exception& e) {
        Rapture::GE_INFO("Error scanning directory: {0}", e.what());
    }
}

void AssetsPanel::render(TestLayer* testLayer) {
    ImGui::Begin("Assets");
    
    // Split the panel into a sidebar and main content area
    float sidebarWidth = 150.0f;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    float mainContentWidth = panelWidth - sidebarWidth;
    
    // Display the sidebar with categories
    ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0), true);
    displaySidebarPanel();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Display main content based on the selected view mode
    ImGui::BeginChild("MainContent", ImVec2(0, 0), true);
    
    switch (m_currentViewMode) {
        case AssetViewMode::Files:
            displayNavigationBar();
            ImGui::Separator();
            displayFileList();
            break;
        case AssetViewMode::Assets:
            displayAssetsList();
            break;
    }
    
    ImGui::EndChild();
    
    // Render import window if active
    if (m_showImportWindow) {
        renderImportWindow(m_importFilePath, m_importFileType);
    }
    
    ImGui::End();
}

void AssetsPanel::displaySidebarPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
    
    // Create a sidebar with category buttons
    const float buttonHeight = 32.0f;
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 buttonSize(availSize.x, buttonHeight);

    // Files Category
    bool filesSelected = (m_currentViewMode == AssetViewMode::Files);
    
    if (ImGui::Button("Files", buttonSize)) {
        m_currentViewMode = AssetViewMode::Files;
    }
    
    // Assets Category
    bool assetsSelected = (m_currentViewMode == AssetViewMode::Assets);
    
    if (ImGui::Button("Assets", buttonSize)) {
        m_currentViewMode = AssetViewMode::Assets;
    }
    
    ImGui::PopStyleVar();
}

void AssetsPanel::displayAssetsList() {
    ImGui::Text("Assets");
    ImGui::Separator();
    
    // Checkbox for showing loaded assets only
    if (ImGui::Checkbox("Show Loaded Assets Only", &m_showLoadedAssetsOnly)) {
        Rapture::GE_INFO("Show loaded assets only: {0}", m_showLoadedAssetsOnly ? "true" : "false");
    }
    
    ImGui::Separator();
    
    // Use the same size and spacing as displayFileList
    const float thumbnailSize = 160.0f;
    const float itemSpacing = 12.0f;
    const float textPadding = 6.0f;
    const float textHeight = ImGui::GetTextLineHeightWithSpacing() * 1.5f;
    const float totalItemHeight = thumbnailSize + textPadding + textHeight;
    
    // Get available panel size
    ImVec2 panelSize = ImGui::GetContentRegionAvail();
    float cursorX = ImGui::GetCursorPosX();
    float cursorY = ImGui::GetCursorPosY();
    float maxX = cursorX + panelSize.x;
    
    try {
        // Get the asset registry and loaded assets
        const auto& assetRegistry = Rapture::AssetManager::getAssetRegistry();
        const auto& loadedAssets = Rapture::AssetManager::getLoadedAssets();
        
        // Layout items in a flow from left to right, wrapping to next line
        bool firstItem = true;
        int index = 0;
        int visibleCount = 0;
        
        for (const auto& [handle, metadata] : assetRegistry) {
            // Skip assets that aren't loaded if the filter is active
            if (m_showLoadedAssetsOnly && loadedAssets.find(handle) == loadedAssets.end()) {
                continue;
            }
            
            // Variables to track ImGui state management
            bool groupBegun = false;
            bool idPushed = false;
            bool fontPushed = false;
            bool fontScaled = false;
            
            try {
                // Get the filename from the metadata path
                std::string filename = metadata.m_filePath.filename().string();
                
                // Skip if empty filename
                if (filename.empty()) {
                    filename = "Unknown";
                }
                
                // If this item would go beyond panel width, move to next line
                if (!firstItem && cursorX + thumbnailSize > maxX) {
                    cursorX = ImGui::GetCursorPosX();
                    cursorY += totalItemHeight + itemSpacing;
                }
                
                // Set item position
                ImGui::SetCursorPos(ImVec2(cursorX, cursorY));
                
                // Unique ID for the item
                ImGui::PushID(index);
                idPushed = true;
                
                // Begin item group (icon + text)
                ImGui::BeginGroup();
                groupBegun = true;
                
                // Remember the position where we start drawing the item
                float itemStartX = cursorX;
                float itemStartY = cursorY;
                
                // Button for the icon
                if (ImGui::Button("##icon", ImVec2(thumbnailSize, thumbnailSize))) {
                    // Handle asset click
                    Rapture::GE_INFO("Asset clicked: {0} (Handle: {1})", filename, handle);
                }
                
                // Right-click context menu
                if (ImGui::BeginPopupContextItem("asset_context_menu")) {
                    if (ImGui::MenuItem("View Details")) {
                        Rapture::GE_INFO("View asset details: {0}", filename);
                    }
                    
                    if (ImGui::MenuItem("Unload Asset")) {
                        Rapture::GE_INFO("Unload asset: {0}", filename);
                    }
                    
                    ImGui::Separator();
                    
                    // Add a menu item for showing the UUID
                    if (ImGui::MenuItem("Show UUID")) {
                        // No direct action, we'll show it on hover
                    }
                    
                    // Show UUID when hovering the "Show UUID" menu item
                    if (ImGui::IsItemHovered()) {
                        // Convert UUID to string
                        std::string fullUUID = std::to_string(handle);
                        
                        // Show full UUID in tooltip and as disabled menu item
                        ImGui::SetTooltip("UUID: %s", fullUUID.c_str());
                        ImGui::Separator();
                        ImGui::BeginDisabled();
                        ImGui::MenuItem(fullUUID.c_str());
                        ImGui::EndDisabled();
                    }
                    
                    ImGui::EndPopup();
                }
                
                // Draw a small indicator if the asset is loaded
                ImVec2 buttonMin = ImGui::GetItemRectMin();
                ImVec2 buttonMax = ImGui::GetItemRectMax();
                ImVec2 buttonSize = ImVec2(buttonMax.x - buttonMin.x, buttonMax.y - buttonMin.y);
                
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                
                // Check if the asset is loaded
                bool isLoaded = loadedAssets.find(handle) != loadedAssets.end();
                
                // Draw a small indicator in the top-right corner if the asset is loaded
                if (isLoaded) {
                    const float indicatorSize = 8.0f;
                    drawList->AddCircleFilled(
                        ImVec2(buttonMax.x - indicatorSize - 4, buttonMin.y + indicatorSize + 4),
                        indicatorSize,
                        IM_COL32(0, 255, 0, 255), // Green for loaded assets
                        8 // segments
                    );
                    
                    // Add drag source for material assets
                    if (metadata.m_assetType == Rapture::AssetType::Material) {
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                            // Set payload to carry the material handle
                            ImGui::SetDragDropPayload("ASSET_MATERIAL", &handle, sizeof(uint64_t));
                            
                            // Preview display while dragging
                            ImGui::Text("Material: %s", filename.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                }
                
                // Use colors from ImGuiPanelStyle where available
                ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
                ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
                ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
                
                // Get asset type name
                std::string assetTypeName;
                switch (metadata.m_assetType) {
                    case Rapture::AssetType::Mesh: assetTypeName = "Mesh"; break;
                    case Rapture::AssetType::Texture2D: assetTypeName = "Texture"; break;
                    case Rapture::AssetType::Material: assetTypeName = "Material"; break;
                    case Rapture::AssetType::Cubemap: assetTypeName = "Cubemap"; break;
                    case Rapture::AssetType::Skeleton: assetTypeName = "Skeleton"; break;
                    case Rapture::AssetType::Animation: assetTypeName = "Animation"; break;
                    case Rapture::AssetType::Audio: assetTypeName = "Audio"; break;
                    case Rapture::AssetType::Script: assetTypeName = "Script"; break;
                    case Rapture::AssetType::Scene: assetTypeName = "Scene"; break;
                    case Rapture::AssetType::Font: assetTypeName = "Font"; break;
                    case Rapture::AssetType::Shader: assetTypeName = "Shader"; break;
                    default: assetTypeName = "Unknown"; break;
                }
                
                // Different colored icons for different asset types
                ImU32 iconColor;
                switch (metadata.m_assetType) {
                    case Rapture::AssetType::Texture2D:
                    case Rapture::AssetType::Cubemap:
                        iconColor = IM_COL32(204, 102, 0, 255); // Darker orange for textures (was 255, 150, 50)
                        break;
                    case Rapture::AssetType::Mesh:
                        iconColor = IM_COL32(50, 150, 255, 255); // Blue for meshes
                        break;
                    case Rapture::AssetType::Material:
                        iconColor = IM_COL32(204, 51, 51, 255); // Darker red for materials (was 255, 100, 100)
                        break;
                    default:
                        iconColor = IM_COL32(140, 140, 140, 255); // Darker gray for others (was 200, 200, 200)
                        break;
                }
                
                // Draw asset icon similar to file icon style
                float iconPosX = buttonMin.x + buttonSize.x * 0.25f;
                float iconPosY = buttonMin.y + buttonSize.y * 0.15f;
                float iconWidth = buttonSize.x * 0.5f;
                float iconHeight = buttonSize.y * 0.7f;
                
                // Background
                drawList->AddRectFilled(
                    ImVec2(iconPosX, iconPosY),
                    ImVec2(iconPosX + iconWidth, iconPosY + iconHeight),
                    iconColor,
                    4.0f // rounded corners
                );
                
                // Type label at the top of the icon
                ImVec2 typeSize = ImGui::CalcTextSize(assetTypeName.c_str());
                drawList->AddText(
                    ImVec2(buttonMin.x + 5, buttonMin.y + 5),
                    textColor,
                    assetTypeName.c_str()
                );
                
                // Enhanced name label below the icon - POSITIONED IMMEDIATELY UNDER THE ICON
                std::string displayName = filename;
                
                // Safety check for empty names
                if (displayName.empty()) {
                    displayName = "[unnamed]";
                }
                
                try {
                    // Use a smaller font for labels to fit more text
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                    ImGui::PushFont(ImGui::GetFont()); // Using current font but applying custom scaling
                    ImGui::SetWindowFontScale(0.9f);   // Scale down text to fit more
                    fontPushed = true;
                    fontScaled = true;
                    
                    // Hard limit for filename length (characters) - increased from 30 to 60
                    const size_t MAX_FILENAME_LENGTH = 60;
                    
                    // If filename exceeds max length, truncate with ellipsis
                    if (displayName.length() > MAX_FILENAME_LENGTH) {
                        // Preserve file extension during truncation
                        std::string basename = displayName;
                        std::string extension = "";
                        
                        // Find the last dot to extract extension
                        size_t lastDot = displayName.find_last_of('.');
                        if (lastDot != std::string::npos && lastDot > 0) {
                            basename = displayName.substr(0, lastDot);
                            extension = displayName.substr(lastDot); // includes the dot
                        }
                        
                        // Keep start of basename and add ellipsis
                        size_t keepLength = MAX_FILENAME_LENGTH - extension.length() - 3; // 3 for "..."
                        if (keepLength < 3) keepLength = 3; // Ensure we show at least 3 chars
                        
                        displayName = basename.substr(0, keepLength) + "..." + extension;
                    }
                    
                    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
                    
                    // Determine if we need wrapping
                    bool needsWrapping = textSize.x > thumbnailSize * 0.95f;
                    
                    // Calculate label dimensions with possible wrapping
                    // Use more of the available space - up to 95% of the thumbnail width
                    float labelWidth = std::min(textSize.x + 16.0f, thumbnailSize * 0.95f);
                    float labelHeight;
                    
                    if (needsWrapping) {
                        // Estimate how many lines we need
                        int estimatedLines = (int)std::ceil(textSize.x / (thumbnailSize * 0.95f));
                        // Cap at max 2 lines
                        estimatedLines = std::min(estimatedLines, 2);
                        labelHeight = textSize.y * estimatedLines + 8.0f;
                    } else {
                        labelHeight = textSize.y + 8.0f;
                    }
                    
                    // Position label directly under the icon
                    float labelX = buttonMin.x + (buttonSize.x - labelWidth) * 0.5f;
                    float labelY = buttonMax.y + 4.0f; // Position directly under the icon with minimal spacing
                    
                    // Set cursor position for the label
                    ImGui::SetCursorPos(ImVec2(labelX, labelY - ImGui::GetCursorPosY()));
                    
                    // Draw the label background with rounded corners
                    ImVec2 labelMin = ImVec2(labelX, labelY);
                    ImVec2 labelMax = ImVec2(labelMin.x + labelWidth, labelMin.y + labelHeight);
                    
                    // Use asset type color for label background
                    drawList->AddRectFilled(
                        labelMin,
                        labelMax,
                        iconColor,
                        4.0f  // Rounded corners
                    );
                    
                    // Draw border
                    drawList->AddRect(
                        labelMin,
                        labelMax,
                        borderColor,
                        4.0f,  // Rounded corners
                        0,     // All corners
                        1.0f   // Border thickness
                    );
                    
                    // Position text inside the label with left alignment
                    ImVec2 textPos;
                    
                    if (needsWrapping) {
                        // For wrapped text, align to left with padding
                        textPos = ImVec2(
                            labelMin.x + 8.0f, // Fixed left padding
                            labelMin.y + 4.0f  // Top padding
                        );
                        
                        // Draw text with wrapping
                        float wrapWidth = labelWidth - 16.0f; // Padding on both sides
                        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.9f,
                            textPos, textColor, displayName.c_str(), nullptr, wrapWidth);
                    } else {
                        // For single-line text, align to left with padding
                        textPos = ImVec2(
                            labelMin.x + 8.0f, // Fixed left padding
                            labelMin.y + (labelHeight - textSize.y) * 0.5f // Keep vertical centering
                        );
                        
                        // Draw the actual text
                        drawList->AddText(
                            textPos,
                            textColor,
                            displayName.c_str()
                        );
                    }
                    
                    // Restore font scaling
                    if (fontScaled) {
                        ImGui::SetWindowFontScale(1.0f);
                        fontScaled = false;
                    }
                    
                    if (fontPushed) {
                        ImGui::PopFont();
                        ImGui::PopStyleVar();
                        fontPushed = false;
                    }
                } catch (const std::exception& e) {
                    Rapture::GE_INFO("Error rendering text: {0}", e.what());
                    
                    // Clean up font state if needed
                    if (fontScaled) {
                        ImGui::SetWindowFontScale(1.0f);
                        fontScaled = false;
                    }
                    
                    if (fontPushed) {
                        ImGui::PopFont();
                        ImGui::PopStyleVar();
                        fontPushed = false;
                    }
                }
                
                // End the group and pop the ID
                if (groupBegun) {
                    ImGui::EndGroup();
                    groupBegun = false;
                }
                
                if (idPushed) {
                    ImGui::PopID();
                    idPushed = false;
                }
                
                // Move cursor position for next item
                cursorX += thumbnailSize + itemSpacing;
                firstItem = false;
                index++;
                visibleCount++;
            } catch (const std::exception& e) {
                Rapture::GE_INFO("Error rendering asset item: {0}", e.what());
                
                // Ensure we clean up all ImGui state in case of an exception
                // Clean up font state if needed
                if (fontScaled) {
                    ImGui::SetWindowFontScale(1.0f);
                }
                
                if (fontPushed) {
                    ImGui::PopFont();
                    ImGui::PopStyleVar();
                }
                
                // End group and pop ID if they were begun/pushed
                if (groupBegun) {
                    ImGui::EndGroup();
                }
                
                if (idPushed) {
                    ImGui::PopID();
                }
                
                // Skip this item and continue with the next
                index++;
                continue;
            }
        }
        
        // Display message if no assets shown due to filter
        if (visibleCount == 0 && m_showLoadedAssetsOnly) {
            ImGui::SetCursorPosY(cursorY + 20);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No loaded assets to display");
            ImGui::TextWrapped("Uncheck 'Show Loaded Assets Only' to view all registered assets");
        }
    } catch (const std::exception& e) {
        Rapture::GE_INFO("Error displaying assets: {0}", e.what());
    }
}

void AssetsPanel::displayNavigationBar() {
    // Back button
    if (ImGui::Button("<")) {
        if (m_currentDirectory != m_rootDirectory) {
            try {
                fs::path currentPath = m_currentDirectory;
                std::string parentPath = currentPath.parent_path().string();
                
                // Verify parent path exists and is accessible
                if (!parentPath.empty() && fs::exists(parentPath) && fs::is_directory(parentPath)) {
                    m_currentDirectory = parentPath;
                    scanCurrentDirectory();
                } else {
                    Rapture::GE_INFO("Invalid parent directory: {0}", parentPath);
                    // Fall back to root
                    m_currentDirectory = m_rootDirectory;
                    scanCurrentDirectory();
                }
            } catch (const std::exception& e) {
                Rapture::GE_INFO("Error navigating up: {0}", e.what());
                // Fall back to root
                m_currentDirectory = m_rootDirectory;
                scanCurrentDirectory();
            }
        }
    }
    ImGui::SameLine();
    
    // Current path display (relative to root directory)
    fs::path currentPath;
    fs::path rootPath;
    
    try {
        currentPath = m_currentDirectory;
        rootPath = m_rootDirectory;
    } catch (const std::exception& e) {
        Rapture::GE_INFO("Error parsing paths: {0}", e.what());
        ImGui::Text("/");
        return;
    }
    
    std::string relativePath;
    if (m_currentDirectory == m_rootDirectory) {
        relativePath = "/";
    } else {
        try {
            // Create a path relative to root directory
            relativePath = "/" + fs::relative(currentPath, rootPath).string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
            
            // If path is too long, truncate it for display
            if (relativePath.length() > 100) {
                relativePath = "/.../"+relativePath.substr(relativePath.length() - 97);
            }
        } catch (const std::exception& e) {
            Rapture::GE_INFO("Error creating relative path: {0}", e.what());
            relativePath = "/";
        }
    }
    
    ImGui::Text("%s", relativePath.c_str());
    
    // Refresh button
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
    if (ImGui::Button("Refresh")) {
        scanCurrentDirectory();
    }
}

void AssetsPanel::displayFileList() {
    // Increased size to 2x original
    const float thumbnailSize = 160.0f;      // Size of the icon (doubled)
    const float itemSpacing = 12.0f;         // Spacing between items
    const float textPadding = 6.0f;          // Padding between icon and text
    const float textHeight = ImGui::GetTextLineHeightWithSpacing() * 1.5f;
    const float totalItemHeight = thumbnailSize + textPadding + textHeight;
    
    // Get available panel size
    ImVec2 panelSize = ImGui::GetContentRegionAvail();
    float cursorX = ImGui::GetCursorPosX();
    float cursorY = ImGui::GetCursorPosY();
    float maxX = cursorX + panelSize.x;
    
    bool firstItem = true;
    
    // Layout items in a flow from left to right, wrapping to next line
    for (int i = 0; i < m_fileItems.size(); i++) {
        // Variables to track ImGui state management
        bool groupBegun = false;
        bool idPushed = false;
        bool fontPushed = false;
        bool fontScaled = false;
        
        try {
            const FileItem& item = m_fileItems[i];
            
            // If this item would go beyond panel width, move to next line
            if (!firstItem && cursorX + thumbnailSize > maxX) {
                cursorX = ImGui::GetCursorPosX();
                cursorY += totalItemHeight + itemSpacing;
            }
            
            // Set item position
            ImGui::SetCursorPos(ImVec2(cursorX, cursorY));
            
            // Unique ID for the item
            ImGui::PushID(i);
            idPushed = true;
            
            // Begin item group (icon + text)
            ImGui::BeginGroup();
            groupBegun = true;
            
            // Remember the position where we start drawing the item
            float itemStartX = cursorX;
            float itemStartY = cursorY;
            
            // Button for the icon
            if (ImGui::Button("##icon", ImVec2(thumbnailSize, thumbnailSize))) {
                if (item.isDirectory) {
                    try {
                        // Verify the directory exists and is accessible
                        if (fs::exists(item.path) && fs::is_directory(item.path)) {
                            m_currentDirectory = item.path;
                            scanCurrentDirectory();
                        } else {
                            Rapture::GE_INFO("Directory no longer accessible: {0}", item.path);
                        }
                    } catch (const std::exception& e) {
                        Rapture::GE_INFO("Error accessing directory: {0}", e.what());
                    }
                } else {
                    // Handle file click - could open file, load it, etc.
                    Rapture::GE_INFO("File clicked: {0}", item.path);
                }
            }
            
            // Right-click context menu - MUST be before popping ID
            if (ImGui::BeginPopupContextItem("item_context_menu")) {
                if (item.isDirectory) {
                    if (ImGui::MenuItem("Open Directory")) {
                        // Same action as clicking the folder
                        try {
                            if (fs::exists(item.path) && fs::is_directory(item.path)) {
                                m_currentDirectory = item.path;
                                scanCurrentDirectory();
                            }
                        } catch (const std::exception& e) {
                            Rapture::GE_INFO("Error accessing directory: {0}", e.what());
                        }
                    }
                    
                    ImGui::Separator();
                } else {
                    // File-specific actions
                    if (isFileSupported(item.path)) {
                        if (ImGui::MenuItem("Import Asset")) {
                            m_showImportWindow = true;
                            m_importFilePath = item.path;
                            m_importFileType = getFileType(item.path);
                        }
                        ImGui::Separator();
                    }

                    if (item.name.find(".gltf") != std::string::npos) { 
                        if (ImGui::MenuItem("Open in Editor")) {
                            Rapture::GE_INFO("Open File action for: {0}", item.path);
                            // Add open file implementation here
                        }
                    }
                }
                
                // Common actions for both files and folders
                if (ImGui::MenuItem("Rename")) {
                    Rapture::GE_INFO("Rename action for: {0}", item.path);
                    // Add rename implementation here
                }
                
                if (ImGui::MenuItem("Delete")) {
                    Rapture::GE_INFO("Delete action for: {0}", item.path);
                    // Add delete implementation here
                }
                
                ImGui::EndPopup();
            }
            
            // Draw the actual icon graphics
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImVec2 buttonSize = ImVec2(buttonMax.x - buttonMin.x, buttonMax.y - buttonMin.y);
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Use ImGui style colors
            ImU32 folderColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            ImU32 fileColor = ImGui::GetColorU32(ImGuiCol_Button);
            ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
            
            if (item.isDirectory) {
                // Draw a folder icon
                ImVec2 folderPos = ImVec2(buttonMin.x + buttonSize.x * 0.2f, buttonMin.y + buttonSize.y * 0.2f);
                ImVec2 folderSize = ImVec2(buttonSize.x * 0.6f, buttonSize.y * 0.5f);
                
                // Folder tab
                drawList->AddRectFilled(
                    ImVec2(folderPos.x, folderPos.y), 
                    ImVec2(folderPos.x + folderSize.x * 0.4f, folderPos.y + folderSize.y * 0.2f),
                    folderColor
                );
                
                // Folder body
                drawList->AddRectFilled(
                    ImVec2(folderPos.x, folderPos.y + folderSize.y * 0.2f), 
                    ImVec2(folderPos.x + folderSize.x, folderPos.y + folderSize.y),
                    folderColor
                );
            } else {
                // Draw a file icon
                ImVec2 filePos = ImVec2(buttonMin.x + buttonSize.x * 0.25f, buttonMin.y + buttonSize.y * 0.15f);
                ImVec2 fileSize = ImVec2(buttonSize.x * 0.5f, buttonSize.y * 0.7f);
                
                // File background
                drawList->AddRectFilled(
                    filePos, 
                    ImVec2(filePos.x + fileSize.x, filePos.y + fileSize.y),
                    fileColor
                );
                
                // File content lines
                float lineHeight = fileSize.y * 0.1f;
                float lineWidth = fileSize.x * 0.75f;
                float lineX = filePos.x + (fileSize.x - lineWidth) * 0.5f;
                float startY = filePos.y + fileSize.y * 0.2f;
                
                ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_Text);
                for (int line = 0; line < 4; line++) {
                    drawList->AddRectFilled(
                        ImVec2(lineX, startY + line * lineHeight * 1.5f),
                        ImVec2(lineX + lineWidth, startY + line * lineHeight * 1.5f + lineHeight),
                        lineColor
                    );
                }
                
                // File corner fold
                drawList->AddTriangleFilled(
                    ImVec2(filePos.x + fileSize.x - fileSize.x * 0.2f, filePos.y),
                    ImVec2(filePos.x + fileSize.x, filePos.y + fileSize.x * 0.2f),
                    ImVec2(filePos.x + fileSize.x, filePos.y),
                    fileColor
                );
            }
            
            // Enhanced name label below the icon - POSITIONED IMMEDIATELY UNDER THE ICON
            std::string displayName = item.name;
            
            // Safety check for empty names
            if (displayName.empty()) {
                displayName = "[unnamed]";
            }
            
            try {
                // Push larger font for labels
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                ImGui::PushFont(ImGui::GetFont()); // Using current font but applying scaling
                ImGui::SetWindowFontScale(1.2f);   // Scale up text
                fontPushed = true;
                fontScaled = true;
                
                ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
                
                // Truncate filename if too long
                if (textSize.x > thumbnailSize * 0.9f) {
                    // Preserve file extension during truncation
                    std::string basename = displayName;
                    std::string extension = "";
                    
                    // Find the last dot to extract extension
                    size_t lastDot = displayName.find_last_of('.');
                    if (lastDot != std::string::npos && lastDot > 0 && !item.isDirectory) {
                        basename = displayName.substr(0, lastDot);
                        extension = displayName.substr(lastDot); // includes the dot
                    }
                    
                    // Calculate how many characters we can show
                    float extensionWidth = extension.empty() ? 0 : ImGui::CalcTextSize(extension.c_str()).x;
                    float ellipsisWidth = ImGui::CalcTextSize("...").x;
                    float availableWidth = thumbnailSize * 0.9f - extensionWidth - ellipsisWidth;
                    
                    // Calculate how many characters of the basename to keep
                    float charWidth = textSize.x / displayName.length();
                    size_t charsToKeep = (size_t)(availableWidth / charWidth);
                    
                    // Ensure we keep at least 3 chars of the basename
                    if (charsToKeep < 3) charsToKeep = 3;
                    if (charsToKeep > basename.length()) charsToKeep = basename.length();
                    
                    displayName = basename.substr(0, charsToKeep) + "..." + extension;
                    textSize = ImGui::CalcTextSize(displayName.c_str());
                }
                
                // Calculate label dimensions
                float labelWidth = textSize.x + 16.0f;
                float labelHeight = textSize.y + 8.0f;
                
                // Position label directly under the icon
                float labelX = buttonMin.x + (buttonSize.x - labelWidth) * 0.5f;
                float labelY = buttonMax.y + 4.0f; // Position directly under the icon with minimal spacing
                
                // Set cursor position for the label
                ImGui::SetCursorPos(ImVec2(labelX, labelY - ImGui::GetCursorPosY()));
                
                // Draw the label background with rounded corners
                ImVec2 labelMin = ImVec2(labelX, labelY);
                ImVec2 labelMax = ImVec2(labelMin.x + labelWidth, labelMin.y + labelHeight);
                
                // Background and text colors from ImGui style
                ImU32 bgColor = item.isDirectory ? 
                                ImGui::GetColorU32(ImGuiCol_ButtonHovered) : 
                                ImGui::GetColorU32(ImGuiCol_Button);
                ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
                                
                drawList->AddRectFilled(
                    labelMin,
                    labelMax,
                    bgColor,
                    4.0f  // Rounded corners
                );
                
                // Draw border
                drawList->AddRect(
                    labelMin,
                    labelMax,
                    borderColor,
                    4.0f,  // Rounded corners
                    0,     // All corners
                    1.0f   // Border thickness
                );
                
                // Position text inside the label with left alignment
                ImVec2 textPos;
                
                if (textSize.x > thumbnailSize * 0.9f) {
                    // For wrapped text, align to left with padding
                    textPos = ImVec2(
                        labelMin.x + 8.0f, // Fixed left padding
                        labelMin.y + 4.0f  // Top padding
                    );
                    
                    // Draw text with wrapping
                    float wrapWidth = labelWidth - 16.0f; // Padding on both sides
                    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.9f,
                        textPos, textColor, displayName.c_str(), nullptr, wrapWidth);
                } else {
                    // For single-line text, align to left with padding
                    textPos = ImVec2(
                        labelMin.x + 8.0f, // Fixed left padding
                        labelMin.y + (labelHeight - textSize.y) * 0.5f // Keep vertical centering
                    );
                    
                    // Draw the actual text
                    drawList->AddText(
                        textPos,
                        textColor,
                        displayName.c_str()
                    );
                }
                
                // Add drag source for files (not directories)
                if (!item.isDirectory) {
                    // Check if it's an image file (for skybox textures)
                    std::string extension = std::filesystem::path(item.path).extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    bool isImageFile = (extension == ".png" || extension == ".jpg" || extension == ".jpeg" 
                                        || extension == ".bmp" || extension == ".tga" || extension == ".hdr");
                    
                    // Make the button a drag source
                    if (isImageFile && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        // Set payload to carry the file path
                        ImGui::SetDragDropPayload("DND_IMAGE_PATH", item.path.c_str(), item.path.length() + 1);
                        
                        // Preview display while dragging
                        ImGui::Text("Dragging: %s", displayName.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                
                // Restore font scaling
                if (fontScaled) {
                    ImGui::SetWindowFontScale(1.0f);
                    fontScaled = false;
                }
                
                if (fontPushed) {
                    ImGui::PopFont();
                    ImGui::PopStyleVar();
                    fontPushed = false;
                }
            } catch (const std::exception& e) {
                Rapture::GE_INFO("Error rendering text: {0}", e.what());
                
                // Clean up font state if needed
                if (fontScaled) {
                    ImGui::SetWindowFontScale(1.0f);
                    fontScaled = false;
                }
                
                if (fontPushed) {
                    ImGui::PopFont();
                    ImGui::PopStyleVar();
                    fontPushed = false;
                }
            }
            
            // End the group and pop the ID
            if (groupBegun) {
                ImGui::EndGroup();
                groupBegun = false;
            }
            
            if (idPushed) {
                ImGui::PopID();
                idPushed = false;
            }
            
            // Move cursor position for next item
            cursorX += thumbnailSize + itemSpacing;
            firstItem = false;
        } catch (const std::exception& e) {
            Rapture::GE_INFO("Error rendering item {0}: {1}", i, e.what());
            
            // Ensure we clean up all ImGui state in case of an exception
            
            // Clean up font state if needed
            if (fontScaled) {
                ImGui::SetWindowFontScale(1.0f);
            }
            
            if (fontPushed) {
                ImGui::PopFont();
                ImGui::PopStyleVar();
            }
            
            // End group and pop ID if they were begun/pushed
            if (groupBegun) {
                ImGui::EndGroup();
            }
            
            if (idPushed) {
                ImGui::PopID();
            }
            
            // Skip this item and continue with the next
            continue;
        }
    }
}

bool AssetsPanel::isFileSupported(const std::string& filePath) const {
    return getFileType(filePath) != ImportFileType::Unknown;
}

ImportFileType AssetsPanel::getFileType(const std::string& filePath) const {
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Check for model files
    if (extension == ".gltf" || extension == ".glb") {
        return ImportFileType::Model;
    }
    // Check for image files
    else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" 
             || extension == ".bmp" || extension == ".tga" || extension == ".hdr") {
        return ImportFileType::Texture;
    }

    return ImportFileType::Unknown;
}

bool AssetsPanel::renderCollapsibleMultiSelect(const char* label, uint8_t* selections, int count, bool* allSelected) {
    bool changed = false;
    
    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        if (ImGui::Checkbox("Select All", allSelected)) {
            for (int i = 0; i < count; i++) {
                selections[i] = *allSelected ? 1 : 0;
            }
            changed = true;
        }
        
        for (int i = 0; i < count; i++) {
            ImGui::PushID(i);
            bool selected = selections[i] != 0;
            if (ImGui::Checkbox(std::to_string(i).c_str(), &selected)) {
                selections[i] = selected ? 1 : 0;
                if (!selected) *allSelected = false;
                else {
                    *allSelected = true;
                    for (int j = 0; j < count; j++) {
                        if (selections[j] == 0) {
                            *allSelected = false;
                            break;
                        }
                    }
                }
                changed = true;
            }
            ImGui::PopID();
        }
        
        ImGui::Unindent();
    } else {
        ImGui::SameLine();
        int selectedCount = 0;
        for (int i = 0; i < count; i++) {
            if (selections[i] != 0) selectedCount++;
        }
        ImGui::Text("(%d/%d selected)", selectedCount, count);
    }
    
    return changed;
}

void AssetsPanel::renderModelImportOptions(const std::string& filePath) {
    // Get model metadata if not already loaded or if file changed
    static std::string lastFile;
    if (lastFile != filePath) {
        m_modelMetadata = Rapture::glTF2Loader::getFileMetadata(filePath, true);
        lastFile = filePath;
        
        // Initialize selection vectors
        m_selectedMaterials.resize(m_modelMetadata.materialCount, true);
        m_selectedPrimitives.resize(m_modelMetadata.primitiveCount, true);
        m_selectedAnimations.resize(m_modelMetadata.animationCount, true);
    }
    
    // Display metadata
    ImGui::Text("Model Information:");
    ImGui::BulletText("Version: %s", m_modelMetadata.version.c_str());
    ImGui::BulletText("Generator: %s", m_modelMetadata.generator.c_str());
    ImGui::BulletText("Nodes: %zu", m_modelMetadata.nodeCount);
    ImGui::BulletText("Meshes: %zu", m_modelMetadata.meshCount);
    ImGui::BulletText("Has Skeleton: %s", m_modelMetadata.hasSkeletons ? "Yes" : "No");
    ImGui::Separator();

    // Import options
    ImGui::Text("Import Options:");
    
    // Materials section
    if (m_modelMetadata.materialCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        renderCollapsibleMultiSelect("Materials", m_selectedMaterials.data(), m_modelMetadata.materialCount, &m_allMaterialsSelected);
        ImGui::PopStyleColor();
    }
    
    // Primitives section
    if (m_modelMetadata.primitiveCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        renderCollapsibleMultiSelect("Primitives", m_selectedPrimitives.data(), m_modelMetadata.primitiveCount, &m_allPrimitivesSelected);
        ImGui::PopStyleColor();
    }
    
    // Animations section
    if (m_modelMetadata.animationCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        renderCollapsibleMultiSelect("Animations", m_selectedAnimations.data(), m_modelMetadata.animationCount, &m_allAnimationsSelected);
        ImGui::PopStyleColor();
    }
    
    ImGui::Separator();
    
    // General import options
    ImGui::Checkbox("Import as Static Mesh", &m_importAsStaticMesh);
    if (m_modelMetadata.materialCount > 0) {
        ImGui::Checkbox("Import Materials", &m_importMaterials);
    }
    if (m_modelMetadata.animationCount > 0 && !m_importAsStaticMesh) {
        ImGui::Checkbox("Import Animations", &m_importAnimations);
    }
}

void AssetsPanel::renderTextureImportOptions() {
    static bool generateMipmaps = true;
    static bool sRGB = true;
    static const char* textureTypes[] = { "Albedo", "Normal", "Roughness", "Metallic", "Height" };
    static int selectedType = 0;

    ImGui::Text("Import Options:");
    ImGui::Checkbox("Generate Mipmaps", &generateMipmaps);
    ImGui::Checkbox("sRGB", &sRGB);
    ImGui::Combo("Texture Type", &selectedType, textureTypes, IM_ARRAYSIZE(textureTypes));
}

void AssetsPanel::renderImportWindow(const std::string& filePath, ImportFileType fileType) {
    if (!m_showImportWindow) return;

    ImGui::SetNextWindowSize(ImVec2(m_importWindowWidth, m_importWindowHeight), ImGuiCond_FirstUseEver);
    
    std::string windowTitle = "Import Asset - " + std::filesystem::path(filePath).filename().string();
    if (ImGui::Begin(windowTitle.c_str(), &m_showImportWindow)) {
        // Display file information
        ImGui::Text("File: %s", std::filesystem::path(filePath).filename().string().c_str());
        ImGui::Text("Type: %s", fileType == ImportFileType::Model ? "Model" : "Texture");
        ImGui::Separator();

        // Different options based on file type
        if (fileType == ImportFileType::Model) {
            renderModelImportOptions(filePath);
        }
        else if (fileType == ImportFileType::Texture) {
            renderTextureImportOptions();
        }

        ImGui::Separator();

        // Import and Cancel buttons at the bottom
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 240 - 10);
        if (ImGui::Button("Import", ImVec2(120, 0))) {
            // TODO: Implement actual import functionality
            m_showImportWindow = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showImportWindow = false;
        }
    }
    ImGui::End();
}
