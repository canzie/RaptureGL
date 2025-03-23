#include "PropertiesPanel.h"
#include "Logger/Log.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameter.h"
#include "Textures/Texture.h"

namespace Rapture {

void PropertiesPanel::render(TestLayer* testLayer) {
    ImGui::Begin("Properties");
    
    // Access the scene registry from the TestLayer
    if (testLayer) {
        auto& registry = testLayer->getActiveScene()->getRegistry();
        
        // Get all entities with transform components
        auto view = registry.view<Rapture::TransformComponent>();
        
        if (view.size() > 0) {
            // Create a vector to store entity handles for UI selection
            std::vector<entt::entity> entities;
            for (auto entity : view) {
                entities.push_back(entity);
            }
            
            // Cap the selected index to valid range
            if (selectedEntityIndex >= entities.size())
                selectedEntityIndex = entities.size() - 1;
                
            // Get the selected entity early for name display
            entt::entity selectedEntity = entities[selectedEntityIndex];
            Rapture::Entity entity(selectedEntity, testLayer->getActiveScene().get());
            
            // Dropdown to select an entity
            if (ImGui::BeginCombo("Entity", entity.hasComponent<Rapture::TagComponent>() ? 
                entity.getComponent<Rapture::TagComponent>().tag.c_str() : 
                std::to_string((uint32_t)entities[selectedEntityIndex]).c_str())) {
                for (int i = 0; i < entities.size(); i++) {
                    Rapture::Entity entityItem(entities[i], testLayer->getActiveScene().get());
                    std::string entityName = entityItem.hasComponent<Rapture::TagComponent>() ? 
                        entityItem.getComponent<Rapture::TagComponent>().tag : 
                        std::to_string((uint32_t)entities[i]);
                    
                    bool isSelected = (selectedEntityIndex == i);
                    if (ImGui::Selectable(entityName.c_str(), isSelected))
                        selectedEntityIndex = i;
                    
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            
            // Edit Transform component
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& transform = entity.getComponent<Rapture::TransformComponent>();
                
                // Position with lock option
                ImGui::BeginGroup();
                
                // Store current colors to restore them later
                ImVec4 origTextColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
                ImVec4 origFrameBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
                ImVec4 origGrabActive = ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive];
                
                // Get translation values to modify them individually
                glm::vec3 position = transform.transforms.getTranslation();
                bool positionChanged = false;
                
                // X axis (Red)
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "X:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.1f, 0.1f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 3.0f - 10.0f);
                if (ImGui::DragFloat("##posX", &position.x, 0.1f)) positionChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // Y axis (Green)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Y:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.6f, 0.1f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.0f - 10.0f);
                if (ImGui::DragFloat("##posY", &position.y, 0.1f)) positionChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // Z axis (Blue)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), "Z:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.6f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.2f, 0.2f, 1.0f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
                if (ImGui::DragFloat("##posZ", &position.z, 0.1f)) positionChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // If position changed, update the transform
                if (positionChanged) {
                    transform.transforms.setTranslation(position);
                    transform.transforms.recalculateTransform();
                }
                
                ImGui::SameLine();
                if (ImGui::Checkbox("##posLock", &positionLocked)) {
                    // Lock state changed
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock position axes");
                ImGui::EndGroup();
                
                // Rotation with lock option
                ImGui::BeginGroup();
                
                // Get rotation values
                glm::vec3 rotation = transform.transforms.getRotation();
                bool rotationChanged = false;
                
                // X rotation (Red)
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "X:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.1f, 0.1f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 3.0f - 10.0f);
                if (ImGui::DragFloat("##rotX", &rotation.x, 0.1f)) rotationChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // Y rotation (Green)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Y:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.6f, 0.1f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.0f - 10.0f);
                if (ImGui::DragFloat("##rotY", &rotation.y, 0.1f)) rotationChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // Z rotation (Blue)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), "Z:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.6f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.2f, 0.2f, 1.0f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
                if (ImGui::DragFloat("##rotZ", &rotation.z, 0.1f)) rotationChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // If rotation changed, update the transform
                if (rotationChanged) {
                    transform.transforms.setRotation(rotation);
                    transform.transforms.recalculateTransform();
                }
                
                ImGui::SameLine();
                if (ImGui::Checkbox("##rotLock", &rotationLocked)) {
                    // Lock state changed
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock rotation axes");
                ImGui::EndGroup();
                
                // Scale with lock option (maintain aspect ratio)
                ImGui::BeginGroup();
                
                // Get scale values
                glm::vec3 scale = transform.transforms.getScale();
                // Store original scale
                glm::vec3 originalScale = scale;
                bool scaleChanged = false;
                
                // X scale (Red)
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "X:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.1f, 0.1f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 3.0f - 10.0f);
                if (ImGui::DragFloat("##scaleX", &scale.x, 0.1f)) scaleChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // Y scale (Green)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Y:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.6f, 0.1f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.0f - 10.0f);
                if (ImGui::DragFloat("##scaleY", &scale.y, 0.1f)) scaleChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // Z scale (Blue)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), "Z:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.6f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.2f, 0.2f, 1.0f, 1.0f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
                if (ImGui::DragFloat("##scaleZ", &scale.z, 0.1f)) scaleChanged = true;
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                
                // If scale changed, update the transform
                if (scaleChanged) {
                    // Apply scale locking if enabled
                    if (scaleLocked) {
                        // Find which component changed
                        float ratioX = originalScale.x != 0.0f ? scale.x / originalScale.x : 1.0f;
                        float ratioY = originalScale.y != 0.0f ? scale.y / originalScale.y : 1.0f;
                        float ratioZ = originalScale.z != 0.0f ? scale.z / originalScale.z : 1.0f;
                        
                        // Determine which component changed the most
                        float ratio = 1.0f;
                        if (abs(ratioX - 1.0f) > abs(ratioY - 1.0f) && abs(ratioX - 1.0f) > abs(ratioZ - 1.0f))
                            ratio = ratioX;
                        else if (abs(ratioY - 1.0f) > abs(ratioZ - 1.0f))
                            ratio = ratioY;
                        else
                            ratio = ratioZ;
                        
                        // Update all components with the same ratio
                        scale = originalScale * ratio;
                    }
                    
                    transform.transforms.setScale(scale);
                    lastScale = scale;
                    transform.transforms.recalculateTransform();
                }
                
                ImGui::SameLine();
                if (ImGui::Checkbox("##scaleLock", &scaleLocked)) {
                    // Lock state changed
                }
                
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock scale (maintain aspect ratio)");
                
                ImGui::EndGroup();
            }
            
            // Edit Material component if it exists
            // Direct check for MaterialComponent
            auto materialView = registry.view<Rapture::MaterialComponent>();
            bool hasMaterial = materialView.contains(selectedEntity);
            
            if (hasMaterial && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& materialComp = registry.get<Rapture::MaterialComponent>(selectedEntity);
                
                // Display material name
                ImGui::Text("Material: %s", materialComp.materialName.c_str());
                
                // Edit base color for any material type
                glm::vec3 baseColor = materialComp.getBaseColor();
                if (ImGui::ColorEdit3("Base Color", glm::value_ptr(baseColor))) {
                    materialComp.setBaseColor(baseColor);
                }
                
                // Only show these properties for PBR materials
                if (materialComp.material && materialComp.material->getType() == Rapture::MaterialType::PBR) {
                    // Edit roughness
                    float roughness = materialComp.getRoughness();
                    if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
                        materialComp.setRoughness(roughness);
                    }
                    
                    // Edit metallic
                    float metallic = materialComp.getMetallic();
                    if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
                        materialComp.setMetallic(metallic);
                    }
                    
                    // Edit specular
                    float specular = materialComp.getSpecular();
                    if (ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f)) {
                        materialComp.setSpecular(specular);
                    }
                }
                
                // Display material textures
                if (materialComp.material) {
                    drawMaterialTextures(entity);
                }
                
                // Material switcher
                if (ImGui::Button("Change Material Type")) {
                    ImGui::OpenPopup("material_type_popup");
                }
                
                if (ImGui::BeginPopup("material_type_popup")) {
                    if (ImGui::MenuItem("Default PBR")) {
                        materialComp = Rapture::MaterialComponent();
                    }
                    
                    if (ImGui::MenuItem("Solid Color")) {
                        materialComp = Rapture::MaterialComponent(materialComp.getBaseColor());
                    }
                    
                    if (ImGui::MenuItem("Custom PBR")) {
                        // Use current values if possible, or defaults
                        glm::vec3 color = materialComp.getBaseColor();
                        float roughness = materialComp.material->getType() == Rapture::MaterialType::PBR ? 
                                        materialComp.getRoughness() : 0.5f;
                        float metallic = materialComp.material->getType() == Rapture::MaterialType::PBR ? 
                                        materialComp.getMetallic() : 0.0f;
                        float specular = materialComp.material->getType() == Rapture::MaterialType::PBR ? 
                                        materialComp.getSpecular() : 0.5f;
                        
                        materialComp = Rapture::MaterialComponent(color, roughness, metallic, specular);
                    }
                    
                    ImGui::EndPopup();
                }
            }
            
            // Edit Light component if it exists
            auto lightView = registry.view<Rapture::LightComponent>();
            bool hasLight = lightView.contains(selectedEntity);
            
            if (hasLight && ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& lightComp = registry.get<Rapture::LightComponent>(selectedEntity);
                
                // Light Type
                const char* lightTypes[] = { "Point", "Directional", "Spot" };
                selectedLightType = static_cast<int>(lightComp.type);
                
                if (ImGui::Combo("Light Type", &selectedLightType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
                    lightComp.type = static_cast<Rapture::LightType>(selectedLightType);
                }
                
                // Light Color
                glm::vec3 lightColor = lightComp.color;
                if (ImGui::ColorEdit3("Light Color", glm::value_ptr(lightColor))) {
                    lightComp.color = lightColor;
                }
                
                // Light Intensity
                float intensity = lightComp.intensity;
                if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 10.0f)) {
                    lightComp.intensity = intensity;
                }
                
                // Range (Point and Spot lights only)
                if (lightComp.type != Rapture::LightType::Directional) {
                    float range = lightComp.range;
                    if (ImGui::SliderFloat("Range", &range, 0.1f, 50.0f)) {
                        lightComp.range = range;
                    }
                }
                
                // Cone Angles (Spot lights only)
                if (lightComp.type == Rapture::LightType::Spot) {
                    // Convert to degrees for editing
                    float innerAngleDegrees = glm::degrees(lightComp.innerConeAngle);
                    float outerAngleDegrees = glm::degrees(lightComp.outerConeAngle);
                    
                    if (ImGui::SliderFloat("Inner Angle", &innerAngleDegrees, 0.0f, outerAngleDegrees)) {
                        lightComp.innerConeAngle = glm::radians(innerAngleDegrees);
                    }
                    
                    if (ImGui::SliderFloat("Outer Angle", &outerAngleDegrees, innerAngleDegrees, 90.0f)) {
                        lightComp.outerConeAngle = glm::radians(outerAngleDegrees);
                    }
                }
                
                // Active/Inactive toggle
                bool isActive = lightComp.isActive;
                if (ImGui::Checkbox("Active", &isActive)) {
                    lightComp.isActive = isActive;
                }
                
                // Add light component button for entities without lights
                ImGui::Separator();
                
                // Button to add lights to objects in the scene
                if (ImGui::Button("Create Light")) {
                    ImGui::OpenPopup("light_creation_popup");
                }
                
                if (ImGui::BeginPopup("light_creation_popup")) {
                    if (ImGui::MenuItem("Point Light")) {
                        // Create a white point light
                        entity.addComponent<Rapture::LightComponent>(
                            glm::vec3(1.0f, 1.0f, 1.0f),  // White color
                            1.0f,                        // Default intensity
                            10.0f                        // Default range
                        );
                    }
                    
                    if (ImGui::MenuItem("Directional Light")) {
                        // Create a white directional light
                        entity.addComponent<Rapture::LightComponent>(
                            glm::vec3(1.0f, 1.0f, 1.0f),  // White color
                            1.0f                         // Default intensity
                        );
                    }
                    
                    if (ImGui::MenuItem("Spot Light")) {
                        // Create a white spot light
                        entity.addComponent<Rapture::LightComponent>(
                            glm::vec3(1.0f, 1.0f, 1.0f),  // White color
                            1.0f,                        // Default intensity
                            10.0f,                       // Default range
                            30.0f,                       // Inner angle (degrees)
                            45.0f                        // Outer angle (degrees)
                        );
                    }
                    
                    ImGui::EndPopup();
                }
            }
            else if (!hasLight) {
                // Button to add a light component if it doesn't exist
                if (ImGui::Button("Add Light Component")) {
                    entity.addComponent<Rapture::LightComponent>();
                }
            }
        } else {
            ImGui::Text("No entities with transform components found.");
        }
    } else {
        ImGui::Text("No active scene available.");
    }
    
    ImGui::End();
}

void PropertiesPanel::drawMaterialTextures(Rapture::Entity& entity) {
    if (!entity.hasComponent<Rapture::MaterialComponent>()) {
        return;
    }
    
    auto& materialComp = entity.getComponent<Rapture::MaterialComponent>();
    
    if (!materialComp.material) {
        return;
    }
    
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Create vectors to store texture names and their parameters
        std::vector<std::pair<std::string, std::shared_ptr<Texture2D>>> textures;
        
        // Common texture parameter names to look for
        static const std::vector<std::string> textureParamNames = {
            "albedoMap", "diffuseMap", "normalMap", "metallicMap", "roughnessMap", 
            "aoMap", "emissiveMap", "specularGlossinessMap", "heightMap"
        };
        
        // Collect all textures from the material
        for (const auto& paramName : textureParamNames) {
            if (materialComp.material->hasParameter(paramName)) {
                const auto& param = materialComp.material->getParameter(paramName);
                if (param.getType() == MaterialParameterType::TEXTURE2D) {
                    auto texture = param.asTexture();
                    if (texture) {
                        textures.push_back({paramName, texture});
                    }
                }
            }
        }
        
        // Display texture list
        if (textures.empty()) {
            ImGui::Text("No textures assigned to this material.");
        } else {
            ImGui::Text("Material Textures:");
            ImGui::Separator();
            
            // Calculate item size for the list
            const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
            
            if (ImGui::BeginListBox("##TexturesList", ImVec2(-FLT_MIN, textures.size() * itemHeight + 10))) {
                for (const auto& [name, texture] : textures) {
                    bool isSelected = (selectedTextureName == name);
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        selectedTextureName = name;
                    }
                    
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndListBox();
            }
            
            // Show texture preview
            if (!selectedTextureName.empty() && materialComp.material->hasParameter(selectedTextureName)) {
                const auto& param = materialComp.material->getParameter(selectedTextureName);
                if (param.getType() == MaterialParameterType::TEXTURE2D) {
                    auto texture = param.asTexture();
                    if (texture) {
                        ImGui::Text("Preview: %s", selectedTextureName.c_str());
                        
                        // Calculate preview size
                        float availWidth = ImGui::GetContentRegionAvail().x;
                        float previewSize = std::min(availWidth, 200.0f);
                        
                        // Get aspect ratio from texture
                        float aspectRatio = static_cast<float>(texture->getWidth()) / static_cast<float>(texture->getHeight());
                        ImVec2 previewDimensions;
                        
                        if (aspectRatio > 1.0f) {
                            previewDimensions = ImVec2(previewSize, previewSize / aspectRatio);
                        } else {
                            previewDimensions = ImVec2(previewSize * aspectRatio, previewSize);
                        }
                        
                        // Display texture as image
                        ImGui::Image((ImTextureID)(uint64_t)texture->getRendererID(), previewDimensions);
                        
                        // Display texture info
                        ImGui::Text("Size: %ux%u", texture->getWidth(), texture->getHeight());
                    }
                }
            }
        }
    }
}

const char* Rapture::PropertiesPanel::getLightTypeString(int type)
{
    switch (type)
    {
    case 0: return "Point";
    case 1: return "Directional";
    case 2: return "Spot";
    default: return "Unknown";
    }
}

}  // namespace Rapture 