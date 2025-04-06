#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm> // For std::transform
#include <cctype>    // For ::tolower
#include <unordered_map>

#include "TestLayer.h"
#include "imgui.h"
#include "File Loaders/glTF/glTF2Loader.h"

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

// Define supported file types for importing
enum class ImportFileType {
    Model,
    Texture,
    Unknown
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
    
    // New helper functions for import functionality
    bool isFileSupported(const std::string& filePath) const;
    ImportFileType getFileType(const std::string& filePath) const;
    void renderImportWindow(const std::string& filePath, ImportFileType fileType);
    void renderModelImportOptions(const std::string& filePath);
    void renderTextureImportOptions();

    // Helper function to render collapsible multi-select header
    bool renderCollapsibleMultiSelect(const char* label, uint8_t* selections, int count, bool* allSelected);
    
    std::string m_rootDirectory;
    std::string m_currentDirectory;
    std::vector<FileItem> m_fileItems;
    AssetViewMode m_currentViewMode = AssetViewMode::Files;
    bool m_showLoadedAssetsOnly = false;

    // Import window state
    bool m_showImportWindow = false;
    std::string m_importFilePath;
    ImportFileType m_importFileType = ImportFileType::Unknown;
    float m_importWindowWidth = 500.0f;
    float m_importWindowHeight = 600.0f;

    // Model import state
    Rapture::glTFMetadata m_modelMetadata;
    std::vector<uint8_t> m_selectedMaterials;
    std::vector<uint8_t> m_selectedPrimitives;
    std::vector<uint8_t> m_selectedAnimations;
    bool m_allMaterialsSelected = true;
    bool m_allPrimitivesSelected = true;
    bool m_allAnimationsSelected = true;
    bool m_importMaterials = true;
    bool m_importAnimations = true;
    bool m_importAsStaticMesh = false;
};

