#include "MaterialViewerPanel.h"
#include "Renderer/Framebuffer.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameter.h"
#include "Textures/Texture.h"
#include "Renderer/PrimitiveShapes.h"
#include "Logger/Log.h"
#include <glm/gtc/type_ptr.hpp>

void MaterialViewerPanel::render(const std::shared_ptr<Rapture::Framebuffer>& framebuffer,
                                const std::shared_ptr<Rapture::Sphere>& sphere) {
    ImGui::Begin("Material Viewer");
    
    // Split the window into two sections
    ImVec2 totalSize = ImGui::GetContentRegionAvail();
    float viewerWidth = totalSize.x * 0.6f; // 60% for the viewer
    float propertiesWidth = totalSize.x - viewerWidth;
    
    // First section: Material preview
    ImGui::BeginChild("MaterialPreviewSection", ImVec2(viewerWidth, 0), true);
    
    // Get the size of the ImGui window viewport
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    // Store the current cursor position - this is the top-left corner of the viewport
    m_viewportPosition = ImGui::GetCursorScreenPos();
    
    if (framebuffer) {
        // Get the texture ID from the framebuffer
        unsigned int textureID = framebuffer->getColorAttachmentRendererID();
        
        // Check if viewport size changed and resize the framebuffer if needed
        if (viewportPanelSize.x != m_lastSize.x || viewportPanelSize.y != m_lastSize.y || m_firstTime) {
            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
                // Update framebuffer size to match viewport
                framebuffer->resize(
                    static_cast<unsigned int>(viewportPanelSize.x), 
                    static_cast<unsigned int>(viewportPanelSize.y));
            }
            m_lastSize = viewportPanelSize;
            m_firstTime = false;
        }
        
        // Display the framebuffer texture in ImGui
        // ImGui::Image uses void* to store the texture ID, so we need to cast it
        ImTextureID texID = (ImTextureID)(intptr_t)textureID;
        ImGui::Image(texID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::Text("Material View not available");
    }
    
    ImGui::EndChild();
    
    // Second section: Material properties
    ImGui::SameLine();
    ImGui::BeginChild("MaterialPropertiesSection", ImVec2(propertiesWidth, 0), true);
    
    // Display material properties if the sphere has a material
    if (sphere && sphere->getMaterial()) {
        auto material = sphere->getMaterial();
        
        // Display material name with a drop target for material assets
        ImGui::Text("Material: ");
        ImGui::SameLine();
        
        // Create material name display with drop target
        const char* materialNameStr = material->getName().c_str();
        if (materialNameStr == nullptr || materialNameStr[0] == '\0') {
            materialNameStr = "[Unnamed Material]";
        }
        
        ImGui::Button(materialNameStr, ImVec2(ImGui::GetContentRegionAvail().x, 0));
        
        // Handle material drag and drop
        handleMaterialDragDrop(sphere);
        
        renderMaterialProperties(material);
    } else {
        ImGui::Text("No material available");
    }
    
    ImGui::EndChild();
    
    ImGui::End();
}

void MaterialViewerPanel::renderMaterialProperties(const std::shared_ptr<Rapture::Material>& material) {
    if (!material) {
        ImGui::Text("Invalid material reference");
        return;
    }
    
    // Display material type
    ImGui::Text("Type: %s", Rapture::MaterialTypeToString(material->getType()).c_str());
    
    ImGui::Separator();
    
    // Edit base color for any material type
    try {
        // Get base color from material
        glm::vec3 baseColor = glm::vec3(1.0f);
        if (material->hasParameter(Rapture::ParameterID::BASE_COLOR)) {
            baseColor = material->getParameter(Rapture::ParameterID::BASE_COLOR).asVec3();
        }
        
        // Edit base color with color picker
        if (ImGui::ColorEdit3("Base Color", glm::value_ptr(baseColor))) {
            material->setVec3(Rapture::ParameterID::BASE_COLOR, baseColor);
        }
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Error getting/setting base color: {}", e.what());
    }
    
    ImGui::Separator();
    
    // Edit PBR properties if this is a PBR material
    if (material->getType() == Rapture::MaterialType::PBR) {
        editPBRProperties(material);
    }
    
    // Edit textures
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        editTextureProperties(material);
    }
}

bool MaterialViewerPanel::handleMaterialDragDrop(const std::shared_ptr<Rapture::Sphere>& sphere) {
    if (!sphere) {
        Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Null sphere reference");
        return false;
    }
    
    // Add drop target functionality
    if (ImGui::BeginDragDropTarget()) {
        // Accept drag drop payload with material handle
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MATERIAL")) {
            // Make sure payload has valid data
            if (!payload || !payload->Data) {
                Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Invalid drag-drop payload");
                ImGui::EndDragDropTarget();
                return false;
            }
            
            // Check data size
            if (payload->DataSize != sizeof(uint64_t)) {
                Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Invalid payload size. Expected size: {0}, received: {1}", 
                    sizeof(uint64_t), payload->DataSize);
                ImGui::EndDragDropTarget();
                return false;
            }
            
            try {
                // Extract the material handle from the payload
                Rapture::AssetHandle materialHandle = *(const Rapture::AssetHandle*)payload->Data;
                
                if (materialHandle == 0) {
                    Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Received invalid zero handle");
                    ImGui::EndDragDropTarget();
                    return false;
                }
                
                // Update the sphere's material
                try {
                    sphere->setMaterial(materialHandle);
                    Rapture::GE_CORE_INFO("Material applied to sphere with handle: {0}", materialHandle);
                }
                catch (const std::exception& e) {
                    Rapture::GE_CORE_ERROR("Failed to set material: {0}", e.what());
                    ImGui::EndDragDropTarget();
                    return false;
                }
                
                ImGui::EndDragDropTarget();
                return true;
            }
            catch (const std::exception& e) {
                Rapture::GE_CORE_ERROR("Exception processing payload: {0}", e.what());
                ImGui::EndDragDropTarget();
                return false;
            }
        }
        
        ImGui::EndDragDropTarget();
    }
    
    return false;
}

void MaterialViewerPanel::editPBRProperties(const std::shared_ptr<Rapture::Material>& material) {
    if (!material) return;
    
    try {
        // Edit roughness
        float roughness = 0.5f;
        if (material->hasParameter(Rapture::ParameterID::ROUGHNESS)) {
            roughness = material->getParameter(Rapture::ParameterID::ROUGHNESS).asFloat();
        }
        
        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
            material->setFloat(Rapture::ParameterID::ROUGHNESS, roughness);
        }
        
        // Edit metallic
        float metallic = 0.0f;
        if (material->hasParameter(Rapture::ParameterID::METALLIC)) {
            metallic = material->getParameter(Rapture::ParameterID::METALLIC).asFloat();
        }
        
        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
            material->setFloat(Rapture::ParameterID::METALLIC, metallic);
        }
        
        // Edit specular
        float specular = 0.5f;
        if (material->hasParameter(Rapture::ParameterID::SPECULAR)) {
            specular = material->getParameter(Rapture::ParameterID::SPECULAR).asFloat();
        }
        
        if (ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f)) {
            material->setFloat(Rapture::ParameterID::SPECULAR, specular);
        }
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Error editing PBR properties: {}", e.what());
    }
}

void MaterialViewerPanel::editTextureProperties(const std::shared_ptr<Rapture::Material>& material) {
    if (!material) return;
    
    // Common texture parameter IDs to look for
    const std::vector<Rapture::ParameterID> textureParamIDs = {
        Rapture::ParameterID::TEXTURE_ALBEDO,
        Rapture::ParameterID::TEXTURE_NORMAL,
        Rapture::ParameterID::TEXTURE_METALLIC,
        Rapture::ParameterID::TEXTURE_ROUGHNESS,
        Rapture::ParameterID::TEXTURE_AO,
        Rapture::ParameterID::TEXTURE_EMISSIVE
    };
    
    // Names for the texture types
    const std::vector<std::string> textureNames = {
        "Albedo Map",
        "Normal Map",
        "Metallic Map",
        "Roughness Map",
        "Ambient Occlusion Map",
        "Emissive Map"
    };
    
    bool hasTextures = false;
    
    // Create a list of available textures
    for (size_t i = 0; i < textureParamIDs.size(); i++) {
        Rapture::ParameterID paramId = textureParamIDs[i];
        const std::string& textureName = textureNames[i];
        
        if (material->hasParameter(paramId)) {
            hasTextures = true;
            
            // Check if the parameter is a texture
            if (material->getParameter(paramId).getType() == Rapture::MaterialParameterType::TEXTURE2D) {
                auto texWeak = material->getParameter(paramId).asTexture();
                auto texture = texWeak.lock();
                
                if (texture) {
                    // List the texture and make it selectable
                    bool isSelected = (m_selectedTextureName == textureName);
                    if (ImGui::Selectable(textureName.c_str(), isSelected)) {
                        m_selectedTextureName = textureName;
                    }
                    
                    // Preview the selected texture
                    if (isSelected) {
                        ImGui::Text("Preview: %s", textureName.c_str());
                        
                        // Calculate preview size
                        float availWidth = ImGui::GetContentRegionAvail().x;
                        float previewSize = std::min(availWidth, 200.0f);
                        
                        // Get aspect ratio from texture
                        float aspectRatio = 1.0f;
                        if (texture->getWidth() > 0 && texture->getHeight() > 0) {
                            aspectRatio = static_cast<float>(texture->getWidth()) / 
                                          static_cast<float>(texture->getHeight());
                        }
                        
                        ImVec2 previewDimensions;
                        if (aspectRatio > 1.0f) {
                            previewDimensions = ImVec2(previewSize, previewSize / aspectRatio);
                        } else {
                            previewDimensions = ImVec2(previewSize * aspectRatio, previewSize);
                        }
                        
                        // Display texture as image
                        uint32_t textureID = texture->getRendererID();
                        ImGui::Image((ImTextureID)(intptr_t)textureID, previewDimensions);
                        
                        // Display texture info
                        ImGui::Text("Size: %ux%u", texture->getWidth(), texture->getHeight());
                    }
                }
            }
        }
    }
    
    if (!hasTextures) {
        ImGui::Text("No textures available");
    }
} 