#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm> // For std::transform
#include <cctype>    // For ::tolower

#include "TestLayer.h"
#include "imgui.h"

struct FileItem {
    std::string name;
    std::string path;
    bool isDirectory;
};

// Define view modes for the assets panel
enum class AssetViewMode {
    Files,
    Assets
};

class AssetsPanel {
public:
    AssetsPanel();
    ~AssetsPanel() = default;

    void render(TestLayer* testLayer);
    void setRootDirectory(const std::string& rootDir);

private:
    void scanCurrentDirectory();
    void displayNavigationBar();
    void displayFileList();
    void displaySidebarPanel();
    void displayAssetsList();
    
    std::string m_rootDirectory;
    std::string m_currentDirectory;
    std::vector<FileItem> m_fileItems;
    AssetViewMode m_currentViewMode = AssetViewMode::Files;
    bool m_showLoadedAssetsOnly = false;
};

