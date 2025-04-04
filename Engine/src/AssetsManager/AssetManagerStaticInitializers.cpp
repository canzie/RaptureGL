#include "AssetManager.h"
#include "AssetImporter.h"

namespace Rapture {

    bool AssetImporter::s_isInitialized = false;
    bool AssetManager::s_isInitialized = false;


    AssetManagerEditor* AssetManager::s_activeAssetManager = nullptr;

}