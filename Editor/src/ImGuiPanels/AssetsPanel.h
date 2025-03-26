#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "TestLayer.h"
#include "imgui.h"

struct FileItem {
    std::string name;
    std::string path;
    bool isDirectory;
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
    
    std::string m_rootDirectory;
    std::string m_currentDirectory;
    std::vector<FileItem> m_fileItems;
};

