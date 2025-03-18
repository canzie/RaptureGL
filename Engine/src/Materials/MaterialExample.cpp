#include "MaterialLibrary.h"
#include "MaterialSerializer.h"
#include "../Logger/Log.h"
#include "../Textures/Texture.h"

namespace Rapture {

// This is an example of how to use the new material system
void MaterialSystemUsageExample()
{
    // Initialize material and texture libraries
    MaterialLibrary::init();
    TextureLibrary::init();
    
    //------------------------------------------------------------
    // Create and use basic materials
    //------------------------------------------------------------
    
    // Create a red PBR material
    auto redMetal = MaterialLibrary::createPBRMaterial("RedMetal", 
        glm::vec3(1.0f, 0.0f, 0.0f),  // Base color (red)
        0.5f,                         // Roughness
        1.0f,                         // Metallic
        0.6f);                        // Specular
    
    // Create a blue plastic material
    auto bluePlastic = MaterialLibrary::createPBRMaterial("BluePlastic", 
        glm::vec3(0.0f, 0.0f, 1.0f),  // Base color (blue)
        0.8f,                         // Roughness
        0.0f,                         // Metallic
        0.5f);                        // Specular
    
    // Create a solid green material
    auto greenSolid = MaterialLibrary::createSolidMaterial("GreenSolid", 
        glm::vec3(0.0f, 1.0f, 0.0f)); // Color (green)
    
    //------------------------------------------------------------
    // Create and use material instances
    //------------------------------------------------------------
    
    // Create a custom instance of the red metal material
    auto customMetal = MaterialLibrary::createMaterialInstance("RedMetal", "CustomMetal");
    if (customMetal) {
        // Override some parameters
        customMetal->setFloat("roughness", 0.8f);
        customMetal->setFloat("metallic", 0.5f);
        
        // Use the material in a rendering context
        customMetal->bind();
        // ... render an object ...
        customMetal->unbind();
    }
    
    //------------------------------------------------------------
    // Using textures with materials
    //------------------------------------------------------------
    
    // Load a texture
    auto diffuseMap = TextureLibrary::load("textures/diffuse.png");
    if (diffuseMap) {
        // Create a textured material
        auto texturedMaterial = MaterialLibrary::createPBRMaterial("TexturedMaterial");
        
        // Set the texture to the material
        texturedMaterial->setTexture("diffuseMap", diffuseMap);
        
        // Use the material
        texturedMaterial->bind();
        // ... render an object ...
        texturedMaterial->unbind();
    }
    
    //------------------------------------------------------------
    // Serialization example
    //------------------------------------------------------------
    
    // Save material to a file
    if (redMetal) {
        MaterialSerializer::saveToFile(redMetal, "materials/red_metal.mat");
    }
    
    // Load material from a file
    auto loadedMaterial = MaterialSerializer::loadFromFile("materials/red_metal.mat");
    if (loadedMaterial) {
        GE_CORE_INFO("Loaded material: {0}", loadedMaterial->getName());
    }
    
    //------------------------------------------------------------
    // Cleanup
    //------------------------------------------------------------
    
    // Shutdown the libraries
    TextureLibrary::shutdown();
    MaterialLibrary::shutdown();
}

} // namespace Rapture 