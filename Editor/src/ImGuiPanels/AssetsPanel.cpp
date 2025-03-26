#include "AssetsPanel.h"
#include "../../Engine/src/Logger/Log.h"

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
    
    displayNavigationBar();
    ImGui::Separator();
    displayFileList();
    
    ImGui::End();
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
        bool stylesPushed = false;
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
            
            // Icon background
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            stylesPushed = true;
            
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
            
            if (stylesPushed) {
                ImGui::PopStyleColor(2);
                stylesPushed = false;
            }
            
            // Draw the actual icon graphics
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImVec2 buttonSize = ImVec2(buttonMax.x - buttonMin.x, buttonMax.y - buttonMin.y);
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            if (item.isDirectory) {
                // Draw a folder icon
                ImVec2 folderPos = ImVec2(buttonMin.x + buttonSize.x * 0.2f, buttonMin.y + buttonSize.y * 0.2f);
                ImVec2 folderSize = ImVec2(buttonSize.x * 0.6f, buttonSize.y * 0.5f);
                
                // Folder tab
                drawList->AddRectFilled(
                    ImVec2(folderPos.x, folderPos.y), 
                    ImVec2(folderPos.x + folderSize.x * 0.4f, folderPos.y + folderSize.y * 0.2f),
                    IM_COL32(80, 140, 200, 255)
                );
                
                // Folder body
                drawList->AddRectFilled(
                    ImVec2(folderPos.x, folderPos.y + folderSize.y * 0.2f), 
                    ImVec2(folderPos.x + folderSize.x, folderPos.y + folderSize.y),
                    IM_COL32(65, 105, 225, 255)
                );
            } else {
                // Draw a file icon
                ImVec2 filePos = ImVec2(buttonMin.x + buttonSize.x * 0.25f, buttonMin.y + buttonSize.y * 0.15f);
                ImVec2 fileSize = ImVec2(buttonSize.x * 0.5f, buttonSize.y * 0.7f);
                
                // File background
                drawList->AddRectFilled(
                    filePos, 
                    ImVec2(filePos.x + fileSize.x, filePos.y + fileSize.y),
                    IM_COL32(220, 220, 220, 255)
                );
                
                // File content lines
                float lineHeight = fileSize.y * 0.1f;
                float lineWidth = fileSize.x * 0.75f;
                float lineX = filePos.x + (fileSize.x - lineWidth) * 0.5f;
                float startY = filePos.y + fileSize.y * 0.2f;
                
                for (int line = 0; line < 4; line++) {
                    drawList->AddRectFilled(
                        ImVec2(lineX, startY + line * lineHeight * 1.5f),
                        ImVec2(lineX + lineWidth, startY + line * lineHeight * 1.5f + lineHeight),
                        IM_COL32(150, 150, 150, 255)
                    );
                }
                
                // File corner fold
                drawList->AddTriangleFilled(
                    ImVec2(filePos.x + fileSize.x - fileSize.x * 0.2f, filePos.y),
                    ImVec2(filePos.x + fileSize.x, filePos.y + fileSize.x * 0.2f),
                    ImVec2(filePos.x + fileSize.x, filePos.y),
                    IM_COL32(180, 180, 180, 255)
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
                
                // Background color - blue for folders, gray for files
                ImU32 bgColor = item.isDirectory ? 
                                IM_COL32(65, 105, 225, 220) :  // Blue for folders
                                IM_COL32(75, 75, 75, 220);     // Gray for files
                                
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
                    IM_COL32(200, 200, 200, 100),
                    4.0f,  // Rounded corners
                    0,     // All corners (replaced ImDrawCornerFlags_All)
                    1.0f   // Border thickness
                );
                
                // Position text inside the label
                ImVec2 textPos = ImVec2(
                    labelMin.x + (labelWidth - textSize.x) * 0.5f,
                    labelMin.y + (labelHeight - textSize.y) * 0.5f
                );
                
                // Draw text with a slight shadow for better readability
                drawList->AddText(
                    ImVec2(textPos.x + 1, textPos.y + 1),
                    IM_COL32(0, 0, 0, 180),
                    displayName.c_str()
                );
                
                // Draw the actual text
                drawList->AddText(
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    displayName.c_str()
                );
                
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
            
            if (stylesPushed) {
                ImGui::PopStyleColor(2);
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
