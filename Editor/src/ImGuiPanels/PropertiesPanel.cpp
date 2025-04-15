#include "PropertiesPanel.h"
#include "Logger/Log.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameter.h"
#include "Textures/Texture.h"


#include "Scenes/Components/Transforms.h"

// Windows file dialog
#include <Windows.h>
#include <commdlg.h>
#include <filesystem>

// ImGuizmo
#include "../vendor/ImGuizmo/ImGuizmo.h"

// Implementation of HelpMarker function
void PropertiesPanel::HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void PropertiesPanel::render(std::shared_ptr<Rapture::Entity> entity) {
    try {
        ImGui::Begin("Properties");
        
        // Access the scene registry from the TestLayer
        if (entity) {
            try {
                renderEntityProperties(entity);
            }
            catch (const std::exception& e) {
                Rapture::GE_CORE_ERROR("Error in renderEntityProperties: {}", e.what());
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error rendering properties");
            }
        } else {
            ImGui::Text("No entity selected");
        }
        
        ImGui::End();
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Critical error in Properties panel: {}", e.what());
        
        // Try to end the ImGui window to avoid ImGui state issues
        try {
            ImGui::End();
        } catch (...) {
            // Ignore any errors from ending the window
        }
    }
}

void PropertiesPanel::render() {
    try {
        ImGui::Begin("Properties");
        ImGui::Text("No entity selected");
        ImGui::End();
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Error in Properties panel: {}", e.what());
        
        // Try to end the ImGui window to avoid ImGui state issues
        try {
            ImGui::End();
        } catch (...) {
            // Ignore any errors from ending the window
        }
    }
}

// Helper method to render entity properties
void PropertiesPanel::renderEntityProperties(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity) {
        Rapture::GE_CORE_ERROR("renderEntityProperties: Attempted to render null entity");
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid entity reference");
        return;
    }
    
    try {
        // Display entity name at the top
        if (entity->hasComponent<Rapture::TagComponent>()) {
            auto& tagComponent = entity->getComponent<Rapture::TagComponent>();
            
            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strncpy_s(buffer, sizeof(buffer), tagComponent.tag.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
                tagComponent.tag = std::string(buffer);
            }
        } else {
            ImGui::Text("Entity ID: %u", entity->getID());
        }
        
        ImGui::Separator();
        
        // Add missing components section
        if (ImGui::CollapsingHeader("Add Components", ImGuiTreeNodeFlags_DefaultOpen)) {
            try {
                // Add Transform component if it doesn't exist
                if (!entity->hasComponent<Rapture::TransformComponent>()) {
                    if (ImGui::Button("Add Transform Component")) {
                        entity->addComponent<Rapture::TransformComponent>();
                        Rapture::GE_CORE_INFO("Added TransformComponent to entity ID {0}", entity->getID());
                    }
                }
                
                // Add Sprite component if entity doesn't have a mesh component
                if (!entity->hasComponent<Rapture::MeshComponent>() && 
                    !entity->hasComponent<Rapture::SpriteComponent>()) {
                    if (ImGui::Button("Add Sprite Component")) {
                        entity->addComponent<Rapture::SpriteComponent>();
                        Rapture::GE_CORE_INFO("Added SpriteComponent to entity ID {0}", entity->getID());
                    }
                }
                
                // Add Mesh component if entity doesn't have a sprite component
                if (!entity->hasComponent<Rapture::SpriteComponent>() && 
                    !entity->hasComponent<Rapture::MeshComponent>()) {
                    if (ImGui::Button("Add Mesh Component")) {
                        //entity->addComponent<Rapture::MeshComponent>();
                        Rapture::GE_CORE_WARN("Add Mesh Component not implemented");
                    }
                }
                
                // Add Material component if it doesn't exist
                if (!entity->hasComponent<Rapture::MaterialComponent>()) {
                    if (ImGui::Button("Add Material Component")) {
                        entity->addComponent<Rapture::MaterialComponent>();
                        Rapture::GE_CORE_INFO("Added MaterialComponent to entity ID {0}", entity->getID());
                    }
                }
                
            }
            catch (const std::exception& e) {
                Rapture::GE_CORE_ERROR("Error adding component: {0}", e.what());
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Failed to add component");
            }
        }
        
        ImGui::Separator();
        
        // Try-catch blocks around each component section to prevent crashes
        
        // Check for AnimationComponent and render if exists
        try {
            if (entity->hasComponent<Rapture::AnimationComponent>() && 
                ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
                renderAnimationControls(entity);
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering animation controls: {}", e.what());
        }
        
        // Check for SkeletonComponent and render if exists
        try {
            if (entity->hasComponent<Rapture::SkeletonComponent>() && 
                ImGui::CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
                renderSkeletonBones(entity);
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering skeleton bones: {}", e.what());
        }
        
        // Edit Sprite component if it exists
        try {
            bool hasSprite = entity->hasComponent<Rapture::SpriteComponent>();
            
            if (hasSprite && ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& spriteComponent = entity->getComponent<Rapture::SpriteComponent>();
                
                // Texture selection
                ImGui::Text("Texture");
                
                // Get current texture
                std::shared_ptr<Rapture::Texture2D> texture = spriteComponent.texture;
                
                // Show texture preview if available
                if (texture && texture) {
                    ImGui::BeginGroup();
                    
                    // Calculate preview size
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    float previewSize = (std::min)(availWidth, 150.0f);
                    
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
                    
                    std::string filename = std::filesystem::path(spriteComponent.texturePath).filename().string();

                    // Display texture info
                    ImGui::Text("Size: %ux%u", texture->getWidth(), texture->getHeight());
                    ImGui::Text("Path: %s", filename.c_str());
                    
                    ImGui::EndGroup();
                } else {
                    ImGui::Text("No texture assigned");
                }
                
                // Button to select texture
                if (ImGui::Button("Select Texture", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    // Windows file dialog implementation
                    OPENFILENAMEA ofn;
                    char szFile[260] = { 0 };
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = nullptr;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = nullptr;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = nullptr;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    // Show the dialog
                    if (GetOpenFileNameA(&ofn)) {
                        std::string filepath = ofn.lpstrFile;
                        
                        // Set the texture
                        spriteComponent.setTexture(filepath);
                        // Log texture loading
                        Rapture::GE_INFO("Loading texture: {}", filepath);
                    }
                }
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering sprite component: {}", e.what());
        }
        
        // Edit Transform component if it exists
        try {
            if (entity->hasComponent<Rapture::TransformComponent>() && 
                ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& transform = entity->getComponent<Rapture::TransformComponent>();
                
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

                    if (entity->hasComponent<Rapture::BoundingBoxComponent>()) {
                        entity->getComponent<Rapture::BoundingBoxComponent>().needsUpdate = true;
                    }

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

                    if (entity->hasComponent<Rapture::BoundingBoxComponent>()) {
                        entity->getComponent<Rapture::BoundingBoxComponent>().needsUpdate = true;
                    }
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
                    if (entity->hasComponent<Rapture::BoundingBoxComponent>()) {
                        entity->getComponent<Rapture::BoundingBoxComponent>().needsUpdate = true;
                    }
                }

                ImGui::SameLine();
                if (ImGui::Checkbox("##scaleLock", &scaleLocked)) {
                    // Lock state changed
                }
                
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock scale (maintain aspect ratio)");
                
                ImGui::EndGroup();
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering transform component: {}", e.what());
        }

        // Add section for BoundingBox component
        try {
            bool hasBoundingBox = entity->hasComponent<Rapture::BoundingBoxComponent>();
            
            if (hasBoundingBox && ImGui::CollapsingHeader("Bounding Box", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& boundingBoxComp = entity->getComponent<Rapture::BoundingBoxComponent>();
                
                // Toggle visibility
                bool isVisible = boundingBoxComp.isVisible;
                if (ImGui::Checkbox("Visible", &isVisible)) {
                    boundingBoxComp.isVisible = isVisible;
                }
                
                // Display bounding box information
                if (boundingBoxComp.worldBoundingBox.isValid()) {
                    glm::vec3 min = boundingBoxComp.worldBoundingBox.getMin();
                    glm::vec3 max = boundingBoxComp.worldBoundingBox.getMax();
                    glm::vec3 size = boundingBoxComp.worldBoundingBox.getSize();
                    
                    ImGui::Text("Min: (%.2f, %.2f, %.2f)", min.x, min.y, min.z);
                    ImGui::Text("Max: (%.2f, %.2f, %.2f)", max.x, max.y, max.z);
                    ImGui::Text("Size: (%.2f, %.2f, %.2f)", size.x, size.y, size.z);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Bounding box is not valid");
                }
                
                // Force update button
                if (ImGui::Button("Update Bounding Box")) {
                    boundingBoxComp.markForUpdate();
                }
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering bounding box component: {}", e.what());
        }

        // Edit Shadow component if it exists
        try {
            bool hasShadow = entity->hasComponent<Rapture::ShadowComponent>();
            
            if (hasShadow && ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen)) {
                renderShadowComponent(entity);
            }
            else if (!hasShadow && entity->hasComponent<Rapture::LightComponent>()) {
                // Button to add a shadow component if it doesn't exist but the entity has a light
                if (ImGui::Button("Add Shadow Component")) {
                    entity->addComponent<Rapture::ShadowComponent>();
                    Rapture::GE_CORE_INFO("Added ShadowComponent to entity {}", 
                        entity->getID());
                }
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering shadow component: {}", e.what());
        }

        // Edit Cascaded Shadow component if it exists
        try {
            bool hasCascadedShadow = entity->hasComponent<Rapture::CascadedShadowComponent>();
            
            if (hasCascadedShadow && ImGui::CollapsingHeader("Cascaded Shadow", ImGuiTreeNodeFlags_DefaultOpen)) {
                renderCascadedShadowComponent(entity);
            }
            else if (!hasCascadedShadow && !entity->hasComponent<Rapture::ShadowComponent>() && 
                     entity->hasComponent<Rapture::LightComponent>()) {
                // Button to add a cascaded shadow component if it doesn't exist, entity has a light,
                // and doesn't already have a regular shadow component
                if (ImGui::Button("Add Cascaded Shadow Component")) {
                    entity->addComponent<Rapture::CascadedShadowComponent>();
                    Rapture::GE_CORE_INFO("Added CascadedShadowComponent to entity {}", 
                        entity->getID());
                }
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering cascaded shadow component: {}", e.what());
        }

        // Edit Material component if it exists
        try {
            bool hasMaterial = entity->hasComponent<Rapture::MaterialComponent>();
            
            if (hasMaterial && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                renderMaterialComponent(entity);
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering material component: {}", e.what());
        }

        // Edit Light component if it exists
        try {
            bool hasLight = entity->hasComponent<Rapture::LightComponent>();
            
            if (hasLight && ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& lightComp = entity->getComponent<Rapture::LightComponent>();
                
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

                // Shadows toggle
                bool castsShadow = lightComp.castsShadow;
                if (ImGui::Checkbox("Cast Shadows", &castsShadow)) {
                    lightComp.castsShadow = castsShadow;
                    
                    // If enabling shadows and no shadow component, suggest adding one
                    if (castsShadow && !entity->hasComponent<Rapture::ShadowComponent>() && 
                        !entity->hasComponent<Rapture::CascadedShadowComponent>()) {
                        
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
                        ImGui::Text("Add a shadow component to enable shadows:");
                        ImGui::PopStyleColor();
                        
                        ImGui::SameLine();
                        
                        if (ImGui::Button("Standard")) {
                            entity->addComponent<Rapture::ShadowComponent>();
                            Rapture::GE_CORE_INFO("Added ShadowComponent to entity {}", entity->getID());
                        }
                        
                        ImGui::SameLine();
                        
                        if (ImGui::Button("Cascaded")) {
                            entity->addComponent<Rapture::CascadedShadowComponent>();
                            Rapture::GE_CORE_INFO("Added CascadedShadowComponent to entity {}", entity->getID());
                        }
                    }
                }
                
                // Show shadow type if shadows are enabled
                if (castsShadow) {
                    ImGui::SameLine();
                    
                    bool hasShadow = entity->hasComponent<Rapture::ShadowComponent>();
                    bool hasCascadedShadow = entity->hasComponent<Rapture::CascadedShadowComponent>();
                    
                    if (hasShadow) {
                        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.2f, 1.0f), "(Standard)");
                        ImGui::SameLine();
                        if (ImGui::Button("Switch to Cascaded")) {
                            entity->removeComponent<Rapture::ShadowComponent>();
                            entity->addComponent<Rapture::CascadedShadowComponent>();
                            Rapture::GE_CORE_INFO("Switched from Standard to Cascaded shadow maps");
                        }
                    }
                    else if (hasCascadedShadow) {
                        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.2f, 1.0f), "(Cascaded)");
                        ImGui::SameLine();
                        if (ImGui::Button("Switch to Standard")) {
                            entity->removeComponent<Rapture::CascadedShadowComponent>();
                            entity->addComponent<Rapture::ShadowComponent>();
                            Rapture::GE_CORE_INFO("Switched from Cascaded to Standard shadow maps");
                        }
                    }
                    else {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(No shadow component)");
                    }
                }
            }
            else if (!hasLight) {
                // Button to add a light component if it doesn't exist
                if (ImGui::Button("Add Light Component")) {
                    entity->addComponent<Rapture::LightComponent>();
                    Rapture::GE_CORE_INFO("Added LightComponent to entity {}", 
                        entity->getID());
                }
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error rendering light component: {}", e.what());
        }
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Critical error rendering entity properties: {}", e.what());
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error rendering properties");
    }
}

void PropertiesPanel::renderMaterialComponent(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity) {
        Rapture::GE_CORE_ERROR("renderMaterialComponent: Null entity reference");
        return;
    }
    
    if (!entity->hasComponent<Rapture::MaterialComponent>()) {
        Rapture::GE_CORE_ERROR("renderMaterialComponent: Entity missing MaterialComponent");
        return;
    }
    
    auto& materialComp = entity->getComponent<Rapture::MaterialComponent>();

    // Display material name with a drop target for material assets
    ImGui::Text("Material: ");
    ImGui::SameLine();
    
    // Create material name display with drop target
    const char* materialNameStr = materialComp.materialName.c_str();
    if (materialComp.materialName.empty()) {
        materialNameStr = "[Unnamed Material]";
    }
    
    ImGui::Button(materialNameStr, ImVec2(ImGui::GetContentRegionAvail().x, 0));
    
    // Handle material drag and drop
    handleMaterialDragDrop(entity);
    
    // Check if material is valid before accessing properties
    if (!materialComp.material) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid material reference");
        
        // Add a button to fix/reset the material
        if (ImGui::Button("Reset Material")) {
            try {
                materialComp = Rapture::MaterialComponent();
                Rapture::GE_CORE_INFO("Material reset to default");
            }
            catch (const std::exception& e) {
                Rapture::GE_CORE_ERROR("Failed to reset material: {}", e.what());
            }
        }
        return;
    }
    
    // Edit base color for any material type
    try {
        glm::vec3 baseColor = materialComp.getBaseColor();
        if (ImGui::ColorEdit3("Base Color", glm::value_ptr(baseColor))) {
            materialComp.setBaseColor(glm::vec4(baseColor, 1.0f));
        }
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("PropertiesPanel::renderMaterialComponent - Error getting/setting base color: {}", e.what());
    }

    // Only show these properties for PBR materials
    if (materialComp.material->getType() == Rapture::MaterialType::PBR) {
        try {
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
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error setting PBR material properties: {}", e.what());
        }
    }

    // Display material textures
    try {
        drawMaterialTextures(entity);
    }
    catch (const std::exception& e) {
        Rapture::GE_CORE_ERROR("Error displaying material textures: {}", e.what());
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Failed to display textures");
    }
    
    // Material switcher
    if (ImGui::Button("Change Material Type")) {
        ImGui::OpenPopup("material_type_popup");
    }
    
    if (ImGui::BeginPopup("material_type_popup")) {
        try {
            if (ImGui::MenuItem("Default PBR")) {
                materialComp = Rapture::MaterialComponent();
                Rapture::GE_CORE_INFO("Material changed to Default PBR");
            }
            
            if (ImGui::MenuItem("Solid Color")) {
                materialComp = Rapture::MaterialComponent(materialComp.getBaseColor());
                Rapture::GE_CORE_INFO("Material changed to Solid Color");
            }
            
            if (ImGui::MenuItem("Custom PBR")) {
                // Use current values if possible, or defaults
                glm::vec3 color = materialComp.getBaseColor();
                float roughness = 0.5f;
                float metallic = 0.0f;
                float specular = 0.5f;
                
                // Only try to get values if the material exists and is PBR
                if (materialComp.material && materialComp.material->getType() == Rapture::MaterialType::PBR) {
                    roughness = materialComp.getRoughness();
                    metallic = materialComp.getMetallic();
                    specular = materialComp.getSpecular();
                }
                
                materialComp = Rapture::MaterialComponent(color, roughness, metallic, specular);
                Rapture::GE_CORE_INFO("Material changed to Custom PBR");
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error changing material type: {}", e.what());
        }
        
        ImGui::EndPopup();
    }
}

bool PropertiesPanel::handleMaterialDragDrop(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity) {
        Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Null entity reference");
        return false;
    }
    
    if (!entity->hasComponent<Rapture::MaterialComponent>()) {
        Rapture::GE_CORE_WARN("handleMaterialDragDrop: Entity does not have a MaterialComponent");
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
            
            // Check data size with simpler format strings
            if (payload->DataSize != sizeof(uint64_t)) {
                Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Invalid payload size. Expected size: {0}, received: {1}", 
                    sizeof(uint64_t), payload->DataSize);
                ImGui::EndDragDropTarget();
                return false;
            }
            
            try {
                // Extract the material handle from the payload
                uint64_t materialHandle = *(const uint64_t*)payload->Data;
                
                if (materialHandle == 0) {
                    Rapture::GE_CORE_ERROR("handleMaterialDragDrop: Received invalid zero handle");
                    ImGui::EndDragDropTarget();
                    return false;
                }
                
                // Update the entity's material component
                auto& materialComp = entity->getComponent<Rapture::MaterialComponent>();
                
                // Try to access the AssetManager
                try {
                    materialComp.setMaterial(materialHandle);
                    // Use simpler format
                    Rapture::GE_CORE_INFO("Material applied with handle: {0}", materialHandle);
                }
                catch (const std::exception& e) {
                    // Use simpler format
                    Rapture::GE_CORE_ERROR("Failed to set material: {0}", e.what());
                    ImGui::EndDragDropTarget();
                    return false;
                }
                
                ImGui::EndDragDropTarget();
                return true;
            }
            catch (const std::exception& e) {
                // Use simpler format
                Rapture::GE_CORE_ERROR("Exception processing payload: {0}", e.what());
                ImGui::EndDragDropTarget();
                return false;
            }
        }
        
        ImGui::EndDragDropTarget();
    }
    
    return false;
}


void PropertiesPanel::drawMaterialTextures(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity) {
        Rapture::GE_CORE_ERROR("drawMaterialTextures: Null entity reference");
        return;
    }
    
    if (!entity->hasComponent<Rapture::MaterialComponent>()) {
        Rapture::GE_CORE_ERROR("drawMaterialTextures: Entity missing MaterialComponent");
        return;
    }
    
    auto& materialComp = entity->getComponent<Rapture::MaterialComponent>();
    
    if (!materialComp.material) {
        Rapture::GE_CORE_WARN("drawMaterialTextures: Invalid material reference");
        return;
    }
    
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Create vectors to store texture names and their parameters
        std::vector<std::pair<std::string, std::shared_ptr<Rapture::Texture2D>>> textures;
        
        // Common texture parameter names to look for
        static const std::vector<std::string> textureParamNames = {
            "albedoMap", "diffuseMap", "normalMap", "metallicMap", "roughnessMap", 
            "aoMap", "emissiveMap", "specularGlossinessMap", "heightMap"
        };
        
        // Collect all textures from the material
        bool hasValidTextures = false;
        for (const auto& paramName : textureParamNames) {
            try {
                if (materialComp.material->hasParameter(paramName)) {
                    const auto& param = materialComp.material->getParameter(paramName);
                    if (param.getType() == Rapture::MaterialParameterType::TEXTURE2D) {
                        auto texture = param.asTexture().lock();
                        if (texture) {
                            textures.push_back({paramName, texture});
                            hasValidTextures = true;
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                // Simplify format string
                Rapture::GE_CORE_ERROR("Error accessing parameter '{0}': {1}", paramName, e.what());
            }
        }
        
        // Display texture list
        if (!hasValidTextures) {
            ImGui::Text("No textures assigned to this material.");
        } else {
            ImGui::Text("Material Textures:");
            ImGui::Separator();
            
            // Calculate item size for the list
            const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
            
            if (ImGui::BeginListBox("##TexturesList", ImVec2(-FLT_MIN, 
                                    (std::min)(textures.size() * itemHeight + 10, 200.0f)))) {
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
            if (!selectedTextureName.empty()) {
                try {
                    if (materialComp.material->hasParameter(selectedTextureName)) {
                        const auto& param = materialComp.material->getParameter(selectedTextureName);
                        if (param.getType() == Rapture::MaterialParameterType::TEXTURE2D) {
                            auto texture = param.asTexture().lock();
                            if (texture) {
                                ImGui::Text("Preview: %s", selectedTextureName.c_str());
                                
                                // Calculate preview size
                                float availWidth = ImGui::GetContentRegionAvail().x;
                                float previewSize = (std::min)(availWidth, 200.0f);
                                
                                // Get aspect ratio from texture
                                float aspectRatio = 1.0f; // Default to 1:1 if we can't get dimensions
                                try {
                                    aspectRatio = static_cast<float>(texture->getWidth()) / 
                                                static_cast<float>(texture->getHeight());
                                }
                                catch (const std::exception& e) {
                                    Rapture::GE_CORE_ERROR("Error getting texture dimensions: {0}", e.what());
                                }
                                
                                ImVec2 previewDimensions;
                                
                                if (aspectRatio > 1.0f) {
                                    previewDimensions = ImVec2(previewSize, previewSize / aspectRatio);
                                } else {
                                    previewDimensions = ImVec2(previewSize * aspectRatio, previewSize);
                                }
                                
                                // Display texture as image
                                try {
                                    uint64_t textureID = texture->getRendererID();
                                    if (textureID != 0) {
                                        ImGui::Image((ImTextureID)textureID, previewDimensions);
                                    } else {
                                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                                          "Invalid texture ID (0)");
                                    }
                                }
                                catch (const std::exception& e) {
                                    Rapture::GE_CORE_ERROR("Error displaying texture: {0}", e.what());
                                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                                      "Failed to display texture");
                                }
                                
                                // Display texture info
                                try {
                                    ImGui::Text("Size: %ux%u", texture->getWidth(), texture->getHeight());
                                }
                                catch (const std::exception& e) {
                                    Rapture::GE_CORE_ERROR("Error getting texture info: {0}", e.what());
                                    ImGui::Text("Size: Unknown");
                                }
                            } else {
                                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                                  "Texture reference is expired or invalid");
                            }
                        } else {
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                              "Parameter is not a texture");
                        }
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                          "Texture parameter no longer exists");
                        // Reset selection
                        selectedTextureName = "";
                    }
                }
                catch (const std::exception& e) {
                    Rapture::GE_CORE_ERROR("Error displaying texture preview: {0}", e.what());
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                      "Error displaying texture preview");
                }
            }
        }
    }
}

const char* PropertiesPanel::getLightTypeString(int type)
{
    switch (type)
    {
    case 0: return "Point";
    case 1: return "Directional";
    case 2: return "Spot";
    default: return "Unknown";
    }
}

void PropertiesPanel::renderSkeletonBones(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity->hasComponent<Rapture::SkeletonComponent>())
        return;
    
    auto& skeletonComponent = entity->getComponent<Rapture::SkeletonComponent>();
    auto& skeleton = skeletonComponent.skeleton;
    
    ImGui::Text("Skeleton Bones");
    ImGui::Separator();
    
    if (ImGui::BeginChild("BonesScrollArea", ImVec2(0, 300), true)) {
        const auto& bones = skeleton->getBones();
        
        for (const auto& bone : bones) {
            if (ImGui::TreeNode(bone->name.c_str())) {
                // Get current transform information
                glm::vec3 translation = glm::vec3(0.0f);
                glm::vec3 rotation = glm::vec3(0.0f);
                glm::vec3 scale = glm::vec3(1.0f);
                
                // Extract translation, rotation, and scale from bone transform matrix using Transforms class
                Rapture::Transforms::decomposeTransform(bone->transform, &translation, &rotation, &scale);
                
                // Convert rotation to degrees for UI display
                rotation = glm::degrees(rotation);
                
                // Translation controls with colored axes
                ImGui::Text("Translation");
                
                // X axis (Red)
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "X:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.1f, 0.1f, 0.5f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 3.0f - 10.0f);
                bool transXChanged = ImGui::DragFloat("##boneTransX", &translation.x, 0.01f);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
                
                // Y axis (Green)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Y:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.6f, 0.1f, 0.5f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.0f - 10.0f);
                bool transYChanged = ImGui::DragFloat("##boneTransY", &translation.y, 0.01f);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
                
                // Z axis (Blue)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), "Z:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.6f, 0.5f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                bool transZChanged = ImGui::DragFloat("##boneTransZ", &translation.z, 0.01f);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
                
                // Rotation controls with colored axes
                ImGui::Text("Rotation");
                
                // X axis (Red)
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "X:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.1f, 0.1f, 0.5f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 3.0f - 10.0f);
                bool rotXChanged = ImGui::DragFloat("##boneRotX", &rotation.x, 0.5f);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
                
                // Y axis (Green)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Y:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.6f, 0.1f, 0.5f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.0f - 10.0f);
                bool rotYChanged = ImGui::DragFloat("##boneRotY", &rotation.y, 0.5f);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
                
                // Z axis (Blue)
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), "Z:");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.6f, 0.5f));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                bool rotZChanged = ImGui::DragFloat("##boneRotZ", &rotation.z, 0.5f);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor();
                
                // If any value has changed, update the bone's transform
                if (transXChanged || transYChanged || transZChanged || rotXChanged || rotYChanged || rotZChanged) {
                    // Convert rotation back to radians
                    glm::vec3 rotationRadians = glm::radians(rotation);
                    
                    // Use Transforms::recalculateTransform to create a new transform matrix
                    glm::mat4 transformMatrix = Rapture::Transforms::recalculateTransform(translation, rotationRadians, scale);
                    
                    // Update the bone's transform
                    bone->transform = transformMatrix;
                    
                    // Propagate the change through the skeleton
                    if (bone->parent) {
                        skeleton->propegateBoneUpdate(bone, bone->parent->worldTransform * bone->transform);
                    } else {
                        skeleton->propegateBoneUpdate(bone, bone->transform);
                    }
                }
                
                ImGui::TreePop();
            }
        }
    }
    ImGui::EndChild();
}

void PropertiesPanel::renderAnimationControls(std::shared_ptr<Rapture::Entity> entity) {
    auto& animComp = entity->getComponent<Rapture::AnimationComponent>();
    
    // Display current animation info
    if (animComp.animation) {
        // Animation details
        ImGui::Text("Current: %s", animComp.animationName.c_str());
        ImGui::Text("Duration: %.2f seconds", animComp.animation->getDuration());
        ImGui::Text("Time: %.2f / %.2f", animComp.animation->getCurrentTime(), animComp.animation->getDuration());
        
        // Playback status
        bool isPlaying = animComp.animation->isPlaying();
        ImGui::Text("Status: %s", isPlaying ? "Playing" : "Paused");
        
        // Playback speed control
        float speed = animComp.animation->getPlaybackSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 2.0f)) {
            animComp.animation->setPlaybackSpeed(speed);
        }
        
        // Looping control
        bool looping = animComp.animation->isLooping();
        if (ImGui::Checkbox("Loop", &looping)) {
            animComp.animation->setLooping(looping);
        }
        
        // Playback controls
        ImGui::Separator();
        
        // Play/Pause button
        if (!isPlaying) {
            if (ImGui::Button("Play", ImVec2(80, 0))) {
                animComp.playAnimation();
            }
        } else {
            if (ImGui::Button("Pause", ImVec2(80, 0))) {
                animComp.pauseAnimation();
            }
        }
        
        ImGui::SameLine();
        
        // Stop button
        if (ImGui::Button("Stop", ImVec2(80, 0))) {
            animComp.stopAnimation();
        }
        
        ImGui::SameLine();
        
        // Reset button
        if (ImGui::Button("Reset", ImVec2(80, 0))) {
            animComp.resetAnimation();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active animation");
    }
    
    // Animation selection dropdown if multiple animations exist
    if (animComp.animations.size() > 1) {
        ImGui::Separator();
        ImGui::Text("Available Animations:");
        
        // Create a vector of animation names for the combo box
        std::vector<const char*> animNames;
        for (const auto& anim : animComp.animations) {
            animNames.push_back(anim->getName().c_str());
        }
        
        // Display combo box with animation selection
        int currentItem = animComp.currentAnimationIndex;
        if (ImGui::Combo("Select Animation", &currentItem, animNames.data(), static_cast<int>(animNames.size()))) {
            animComp.setAnimation(currentItem);
        }
    }
}

void PropertiesPanel::renderShadowComponent(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity) {
        Rapture::GE_CORE_ERROR("renderShadowComponent: Null entity reference");
        return;
    }
    
    if (!entity->hasComponent<Rapture::ShadowComponent>()) {
        Rapture::GE_CORE_ERROR("renderShadowComponent: Entity missing ShadowComponent");
        return;
    }
    
    auto& shadowComp = entity->getComponent<Rapture::ShadowComponent>();
    
    // Toggle shadow active state
    bool isActive = shadowComp.isActive;
    if (ImGui::Checkbox("Active", &isActive)) {
        shadowComp.isActive = isActive;
    }
    
    // Display and edit shadow map resolution
    ImGui::Text("Shadow Map Resolution:");
    
    
    // Add control for shadow map size (for directional lights)
    // This controls the orthographic projection size
    ImGui::Separator();
    ImGui::Text("Shadow Map Settings:");
    
    // Check if this entity has a light component and it's a directional light
    bool isDirectionalLight = false;
    if (entity->hasComponent<Rapture::LightComponent>()) {
        auto& lightComp = entity->getComponent<Rapture::LightComponent>();
        isDirectionalLight = (lightComp.type == Rapture::LightType::Directional);
    }
    
    // Only show this for directional lights
    if (isDirectionalLight) {
        float shadowSize = shadowComp.shadowMapSize;
        if (ImGui::DragFloat("Coverage Size", &shadowSize, 1.0f, 10.0f, 500.0f, "%.1f")) {
            shadowComp.shadowMapSize = shadowSize;
        }
        ImGui::SameLine();
        HelpMarker("Controls how much area the directional light shadow covers. Higher values show more of the scene but with less detail.");
    }
    
    // Shadow map preview
    ImGui::Separator();
    ImGui::Text("Shadow Map Preview:");
    
    if (shadowComp.shadowMap) {
        try {
            // Get the shadow map texture ID
            uint32_t shadowMapID = shadowComp.shadowMap->getShadowMapID();
            
            if (shadowMapID > 0) {
                // Calculate preview size
                float availWidth = ImGui::GetContentRegionAvail().x;
                float previewSize = std::min(availWidth, 312.0f);
                
                // Shadow maps are typically square, but use 1:1 aspect ratio just in case
                ImVec2 previewDimensions(previewSize, previewSize);
                
                // Display the depth texture with zoom controls
                static float zoomLevel = 1.0f;
                if (ImGui::SliderFloat("Zoom", &zoomLevel, 0.5f, 3.0f, "%.1fx")) {
                    // Zoom level changed
                }
                
                // Display zoomed image
                ImVec2 uv0 = ImVec2(0.5f - 0.5f/zoomLevel, 0.5f - 0.5f/zoomLevel);
                ImVec2 uv1 = ImVec2(0.5f + 0.5f/zoomLevel, 0.5f + 0.5f/zoomLevel);
                
                ImGui::Image((ImTextureID)(uint64_t)shadowMapID, previewDimensions, 
                             uv0, uv1, 
                             ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                
                // Display texture info
                ImGui::Text("Shadow Map ID: %u", shadowMapID);
                ImGui::Text("Resolution: %ux%u", shadowComp.width, shadowComp.height);
                if (isDirectionalLight) {
                    ImGui::Text("Coverage: %.1f units", shadowComp.shadowMapSize);
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid shadow map texture ID");
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error displaying shadow map: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Error displaying shadow map");
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Shadow map not initialized");
        

    }
}

void PropertiesPanel::renderCascadedShadowComponent(std::shared_ptr<Rapture::Entity> entity) {
    if (!entity) {
        Rapture::GE_CORE_ERROR("renderCascadedShadowComponent: Null entity reference");
        return;
    }
    
    if (!entity->hasComponent<Rapture::CascadedShadowComponent>()) {
        Rapture::GE_CORE_ERROR("renderCascadedShadowComponent: Entity missing CascadedShadowComponent");
        return;
    }
    
    auto& csmComp = entity->getComponent<Rapture::CascadedShadowComponent>();
    
    // Toggle shadow active state
    bool isActive = csmComp.isActive;
    if (ImGui::Checkbox("Active", &isActive)) {
        csmComp.isActive = isActive;
    }
    
    // Display resolution and cascade count
    ImGui::Text("Resolution: %ux%u", csmComp.width, csmComp.height);
    ImGui::Text("Number of Cascades: %u", csmComp.numCascades);
    
    // Shadow map preview
    ImGui::Separator();
    ImGui::Text("Cascade Shadow Map Previews:");
    
    if (csmComp.cascadedShadowMapping) {
        try {
            // Get the shadow map texture IDs for all cascades
            std::vector<uint32_t> textureIDs = csmComp.cascadedShadowMapping->getCascadeTextureIDs();
            
            if (!textureIDs.empty()) {
                // Calculate preview size and layout
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                // Number of cascades per row (2 for 4 cascades, 2 for 3 cascades, etc.)
                int cascadesPerRow = 2;
                int numCascades = textureIDs.size();
                
                // Adjust preview size based on available width
                float padding = 10.0f;
                float previewSize = std::min((availWidth - (cascadesPerRow - 1) * padding) / cascadesPerRow, 312.0f);
                
                // Display each cascade shadow map
                for (int i = 0; i < numCascades; ++i) {
                    // Start a new row if needed
                    if (i > 0 && i % cascadesPerRow == 0) {
                        ImGui::NewLine();
                    } else if (i > 0) {
                        ImGui::SameLine(i % cascadesPerRow * (previewSize + padding));
                    }
                    
                    // Create a group for each cascade
                    ImGui::BeginGroup();
                    
                    // Display the cascade number
                    ImGui::Text("Cascade %d", i + 1);
                    
                    // Create preview dimensions (shadow maps are typically square)
                    ImVec2 previewDimensions(previewSize, previewSize);
                    
                    // Display the depth texture
                    uint32_t textureID = textureIDs[i];
                    if (textureID > 0) {
                        ImGui::Image(
                            (ImTextureID)(uint64_t)textureID, 
                            previewDimensions, 
                            ImVec2(0, 0), ImVec2(1, 1), 
                            ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f)
                        );
                    } else {
                        ImGui::Text("Invalid texture ID");
                    }
                    
                    ImGui::EndGroup();
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No cascade textures available");
            }
        }
        catch (const std::exception& e) {
            Rapture::GE_CORE_ERROR("Error displaying cascade shadow maps: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Error displaying cascade shadow maps");
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Cascaded shadow maps not initialized");
        
        // Add a button to create the shadow map
        if (ImGui::Button("Create Shadow Maps")) {
            try {
                csmComp.cascadedShadowMapping = std::make_shared<Rapture::CascadedShadowMapping>(
                    csmComp.width, csmComp.height, csmComp.numCascades);
                csmComp.isActive = true;
                Rapture::GE_CORE_INFO("Created cascaded shadow maps with resolution {}x{} and {} cascades", 
                                      csmComp.width, csmComp.height, csmComp.numCascades);
            }
            catch (const std::exception& e) {
                Rapture::GE_CORE_ERROR("Failed to create cascaded shadow maps: {}", e.what());
            }
        }
    }
}
