#include "RenderQueue.h"
#include "../Scenes/Components/Components.h"
#include <unordered_set>

namespace Rapture {

    // Helper function to check if an entity or any of its ancestors has a skeleton
    static bool hasSkeletonInHierarchy(const std::shared_ptr<EntityNode>& node) {
        if (!node) return false;
        
        // Check if the current entity has a skeleton
        auto entity = node->getEntity();
        if (entity->hasComponent<SkeletonComponent>()) {
            return true;
        }
        
        // Check parent recursively
        auto parent = node->getParent();
        if (parent) {
            return hasSkeletonInHierarchy(parent);
        }
        
        return false;
    }
    
    // Helper function to find the skeleton & animation at the root of a hierarchy
    static std::pair<std::shared_ptr<Skeleton>, std::shared_ptr<Animation>> 
    findSkeletonInHierarchy(const std::shared_ptr<EntityNode>& node) {
        if (!node) return {nullptr, nullptr};
        
        // Check if the current entity has a skeleton
        auto entity = node->getEntity();
        if (entity->hasComponent<SkeletonComponent>()) {
            auto& skeletonComp = entity->getComponent<SkeletonComponent>();
            
            // Find animation if available
            std::shared_ptr<Animation> animation = nullptr;
            if (entity->hasComponent<AnimationComponent>()) {
                auto& animComp = entity->getComponent<AnimationComponent>();
                animation = animComp.animation;
            }
            
            return {skeletonComp.skeleton, animation};
        }
        
        // Check parent recursively
        auto parent = node->getParent();
        if (parent) {
            return findSkeletonInHierarchy(parent);
        }
        
        return {nullptr, nullptr};
    }

    RenderQueue CommandQueueBuilder::buildGeometryCommandQueue(const std::shared_ptr<Scene> &scene)
    {
        RenderQueue queue("GeometryQueue", RenderQueueType::GEOMETRY);
        auto& reg = scene->getRegistry();
        auto& sceneConfig = scene->getSettings();
        
        // Track processed entities to avoid duplicates
        std::unordered_set<uint32_t> processedEntities;
        
        // Step 1: Process skeletal hierarchies first
        // These are entities with both Skeleton and EntityNode components
        auto skeletalHierarchies = reg.view<SkeletonComponent, EntityNodeComponent>();
        
        for (auto entityHandle : skeletalHierarchies) {
            Entity entity(entityHandle, scene.get());
            auto& nodeComp = entity.getComponent<EntityNodeComponent>();
            auto& skeletonComp = entity.getComponent<SkeletonComponent>();
            
            // Get animation if available
            std::shared_ptr<Animation> animation = nullptr;
            if (entity.hasComponent<AnimationComponent>()) {
                auto& animComp = entity.getComponent<AnimationComponent>();
                animation = animComp.animation;
            }
            
            // Create animation setup command
            AnimationSetupCommand setupCmd;
            setupCmd.skeleton = skeletonComp.skeleton;
            setupCmd.animation = animation;
            
            // First, add the animation setup command
            queue.add(setupCmd);
            
            // Process all children in the hierarchy that have render components
            std::function<void(const std::shared_ptr<EntityNode>&)> processNode = 
            [&](const std::shared_ptr<EntityNode>& node) {
                auto childEntity = node->getEntity();
                uint32_t entityId = childEntity->getID();
                
                // Skip if already processed
                if (processedEntities.count(entityId) > 0) {
                    return;
                }
                
                processedEntities.insert(entityId);
                
                // If the entity has required rendering components, add a render command
                if (childEntity->hasAllComponents<TransformComponent, MeshComponent, MaterialComponent>()) {
                    auto& transform = childEntity->getComponent<TransformComponent>();
                    auto& mesh = childEntity->getComponent<MeshComponent>();
                    auto& material = childEntity->getComponent<MaterialComponent>();
                    
                    // Skip if mesh is still loading
                    if (!mesh.isLoading) {
                        RenderCommand command;
                        command.entity = childEntity;
                        command.material = material.material;
                        command.mesh = mesh.mesh;
                        command.transform = transform.transformMatrix();
                        command.isSkeletal = true;
                        command.skeleton = skeletonComp.skeleton;
                        
                        queue.add(command);
                    }
                }
                
                // Process all children recursively
                for (auto& child : node->getChildren()) {
                    processNode(child);
                }
            };
            
            // Start processing from the skeleton entity node
            processNode(nodeComp.entity_node);
        }
        
        // Step 2: Process standalone hierarchical entities that don't have skeletons
        auto hierarchyEntities = reg.view<EntityNodeComponent>(entt::exclude<SkeletonComponent>);
        
        for (auto entityHandle : hierarchyEntities) {
            Entity entity(entityHandle, scene.get());
            uint32_t entityId = entity.getID();
            
            // Skip if already processed
            if (processedEntities.count(entityId) > 0) {
                continue;
            }
            
            auto& nodeComp = entity.getComponent<EntityNodeComponent>();
            
            // Process all renderable entities in this hierarchy
            std::function<void(const std::shared_ptr<EntityNode>&)> processNode = 
            [&](const std::shared_ptr<EntityNode>& node) {
                auto childEntity = node->getEntity();
                uint32_t childId = childEntity->getID();
                
                // Skip if already processed
                if (processedEntities.count(childId) > 0) {
                    return;
                }
                
                processedEntities.insert(childId);
                
                // If the entity has required rendering components, add a render command
                if (childEntity->hasAllComponents<TransformComponent, MeshComponent, MaterialComponent>()) {
                    auto& transform = childEntity->getComponent<TransformComponent>();
                    auto& mesh = childEntity->getComponent<MeshComponent>();
                    auto& material = childEntity->getComponent<MaterialComponent>();
                    
                    // Skip if mesh is still loading
                    if (!mesh.isLoading) {
                        RenderCommand command;
                        command.entity = childEntity;
                        command.material = material.material;
                        command.mesh = mesh.mesh;
                        command.transform = transform.transformMatrix();
                        command.isSkeletal = false;
                        
                        queue.add(command);
                    }
                }
                
                // Process all children recursively
                for (auto& child : node->getChildren()) {
                    processNode(child);
                }
            };
            
            // Start processing from this node
            processNode(nodeComp.entity_node);
        }
        
        // Step 3: Process regular mesh entities (no hierarchy, no skeleton)
        auto regularEntities = reg.view<TransformComponent, MeshComponent, MaterialComponent>(
            entt::exclude<SkeletonComponent, EntityNodeComponent>);
        
        for (auto entityHandle : regularEntities) {
            Entity entity(entityHandle, scene.get());
            uint32_t entityId = entity.getID();
            
            // Skip if already processed
            if (processedEntities.count(entityId) > 0) {
                continue;
            }
            
            auto& transform = entity.getComponent<TransformComponent>();
            auto& mesh = entity.getComponent<MeshComponent>();
            auto& material = entity.getComponent<MaterialComponent>();
            
            // Skip if mesh is still loading
            if (mesh.isLoading) {
                continue;
            }
            
            RenderCommand command;
            command.entity = std::make_shared<Entity>(entity);
            command.material = material.material;
            command.mesh = mesh.mesh;
            command.transform = transform.transformMatrix();
            command.isSkeletal = false;
            
            queue.add(command);
        }
        
        return queue;
    }

    RenderQueue CommandQueueBuilder::buildPostProcessCommandQueue(const std::shared_ptr<Scene> &scene)
    {
        return RenderQueue("PostProcessQueue", RenderQueueType::POSTPROCESS);
    }
}
