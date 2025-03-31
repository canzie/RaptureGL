#include "RenderQueue.h"
#include "../Scenes/Components/Components.h"
#include <unordered_set>

namespace Rapture {

    // Initialize static members
    std::atomic_bool CommandQueueBuilder::s_initialized(false);
    std::atomic_bool CommandQueueBuilder::s_shuttingDown(false);
    std::vector<std::thread> CommandQueueBuilder::s_workerThreads;
    std::mutex CommandQueueBuilder::s_queueMutex;
    std::condition_variable CommandQueueBuilder::s_queueCV;
    std::queue<QueueBuildRequest> CommandQueueBuilder::s_pendingBuilds;

    // Helper function to check if an entity or any of its ancestors has a skeleton
    static bool hasSkeletonInHierarchy(const std::shared_ptr<EntityNode>& node) {
        RAPTURE_PROFILE_SCOPE("hasSkeletonInHierarchy");

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

    void CommandQueueBuilder::init(unsigned int numThreads)
    {
        RAPTURE_PROFILE_FUNCTION();
        GE_RENDER_INFO("CommandQueueBuilder: Initializing with {} worker threads", numThreads);
        
        if (s_initialized) {
            GE_RENDER_WARN("CommandQueueBuilder already initialized");
            return;
        }
        
        // Prevent startup during shutdown
        if (s_shuttingDown) {
            GE_RENDER_ERROR("Cannot initialize CommandQueueBuilder during shutdown");
            return;
        }
        
        // Create worker threads
        s_shuttingDown = false;
        s_initialized = true;
        
        // Limit to hardware concurrency if needed
        unsigned int maxThreads = std::thread::hardware_concurrency();
        if (numThreads > maxThreads) {
            GE_RENDER_WARN("Requested {} threads, limiting to {} hardware threads", numThreads, maxThreads);
            numThreads = maxThreads;
        }
        
        // Ensure at least one thread
        numThreads = std::max(1u, numThreads);
        
        // Create worker threads
        s_workerThreads.resize(numThreads);
        for (unsigned int i = 0; i < numThreads; ++i) {
            s_workerThreads[i] = std::thread(queueBuilderThread);
        }
        
        GE_RENDER_INFO("CommandQueueBuilder: Initialized with {} worker threads", numThreads);
    }

    void CommandQueueBuilder::queueBuilderThread()
    {
        RAPTURE_PROFILE_THREAD("QueueBuilderWorker");
        GE_RENDER_INFO("CommandQueueBuilder: Worker thread started");
        
        while (!s_shuttingDown) {
            QueueBuildRequest request;
            bool hasRequest = false;
            
            // Wait for work or shutdown signal
            {
                std::unique_lock<std::mutex> lock(s_queueMutex);
                s_queueCV.wait(lock, [] {
                    return !s_pendingBuilds.empty() || s_shuttingDown;
                });
                
                // Check if we should exit
                if (s_shuttingDown) break;
                
                // Get next request
                if (!s_pendingBuilds.empty()) {
                    request = s_pendingBuilds.front();
                    s_pendingBuilds.pop();
                    hasRequest = true;
                }
            }
            
            // Process request outside of lock
            if (hasRequest && request.scene && request.resultQueue) {
                RAPTURE_PROFILE_SCOPE("Process Queue Build Request");
                
                try {
                    // Build geometry queue
                    buildGeometryQueue(request);
                }
                catch (const std::exception& e) {
                    GE_RENDER_ERROR("Exception in queue builder thread: {}", e.what());
                    request.resultQueue->markAsDone();
                }
                catch (...) {
                    GE_RENDER_ERROR("Unknown exception in queue builder thread");
                    request.resultQueue->markAsDone();
                }
            }
        }
        
        GE_RENDER_INFO("CommandQueueBuilder: Worker thread stopped");
    }

    void CommandQueueBuilder::buildGeometryQueue(const QueueBuildRequest& request)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        auto& scene = request.scene;
        auto& queue = request.resultQueue;
        auto& reg = scene->getRegistry();
        
        // Track processed entities to avoid duplicates
        std::unordered_set<uint32_t> processedEntities;
        
        // Step 1: Process skeletal hierarchies first
        {
            RAPTURE_PROFILE_SCOPE("Process Skeletal Hierarchies");
            auto skeletalView = reg.view<SkeletonComponent, EntityNodeComponent>();
            
            for (auto entityHandle : skeletalView) {
                // Check for shutdown
                if (s_shuttingDown) {
                    queue->markAsDone();
                    return;
                }
                
                uint32_t entityId = static_cast<uint32_t>(entityHandle);
                // Skip if already processed
                if (processedEntities.count(entityId) > 0) {
                    continue;
                }
                
                processedEntities.insert(entityId);
                
                // Get components directly from view
                auto& nodeComp = skeletalView.get<EntityNodeComponent>(entityHandle);
                auto& skeletonComp = skeletalView.get<SkeletonComponent>(entityHandle);
                

                // Get animation if available
                std::shared_ptr<Animation> animation = nullptr;
                if (reg.all_of<AnimationComponent>(entityHandle)) {
                    animation = reg.get<AnimationComponent>(entityHandle).animation;
                }
                
                // Create animation setup command
                AnimationSetupCommand setupCmd;
                setupCmd.skeleton = skeletonComp.skeleton;
                setupCmd.animation = animation;
                
                // Add the animation setup command
                queue->add(setupCmd);
                
                // Process all children in the hierarchy
                std::function<void(const std::shared_ptr<EntityNode>&)> processNode = 
                [&](const std::shared_ptr<EntityNode>& node) {
                    RAPTURE_PROFILE_SCOPE("Process Node");

                    auto childEntityPtr = node->getEntity();
                    if (!childEntityPtr) return;
                    

                    auto childHandle = childEntityPtr->m_EntityHandle;
                    uint32_t childId = childEntityPtr->getID();
                    


                    // Skip if already processed
                    if (processedEntities.count(childId) > 0) {
                        return;
                    }

                    
                    processedEntities.insert(childId);
                    
                    // Check for render components
                    if (reg.all_of<TransformComponent, MeshComponent, MaterialComponent>(childHandle)) {
                        auto& transform = reg.get<TransformComponent>(childHandle);
                        auto& mesh = reg.get<MeshComponent>(childHandle);
                        auto& material = reg.get<MaterialComponent>(childHandle);
                        

                        // Skip if mesh is still loading
                        if (!mesh.isLoading) {
                            RenderCommand command;
                            command.entity = childEntityPtr;
                            command.material = material.material;
                            command.mesh = mesh.mesh;
                            command.transform = transform.transformMatrix();
                            command.isSkeletal = true;
                            
                            queue->add(command);
                        }
                    }
                    

                    // Process all children recursively
                    for (auto& child : node->getChildren()) {

                        processNode(child);
                    }
                };
                
                if (nodeComp.entity_node->getChildren().size() > 0) {
                    // Start processing from the skeleton entity node
                    processNode(nodeComp.entity_node->getChildren()[0]);
                }
            }
        }
        
        // Step 2: Process hierarchical entities (no skeleton)
        {
            RAPTURE_PROFILE_SCOPE("Process Hierarchical Entities");
            auto hierarchyView = reg.view<EntityNodeComponent>(entt::exclude<SkeletonComponent>);
            
            for (auto entityHandle : hierarchyView) {
                // Check for shutdown
                if (s_shuttingDown) {
                    queue->markAsDone();
                    return;
                }
                
                uint32_t entityId = static_cast<uint32_t>(entityHandle);
                
                // Skip if already processed
                if (processedEntities.count(entityId) > 0) {
                    continue;
                }
                
                auto& nodeComp = hierarchyView.get<EntityNodeComponent>(entityHandle);
                
                // Process all renderable entities in hierarchy
                std::function<void(const std::shared_ptr<EntityNode>&)> processNode = 
                [&](const std::shared_ptr<EntityNode>& node) {
                    RAPTURE_PROFILE_SCOPE("Process Node");
                    auto childEntityPtr = node->getEntity();
                    if (!childEntityPtr) return;
                    
                    auto childHandle = static_cast<entt::entity>(*childEntityPtr);
                    uint32_t childId = static_cast<uint32_t>(childHandle);
                    
                    // Skip if already processed
                    if (processedEntities.count(childId) > 0) {
                        return;
                    }
                    
                    processedEntities.insert(childId);
                    
                    // Check for render components
                    if (reg.all_of<TransformComponent, MeshComponent, MaterialComponent>(childHandle)) {
                        auto& transform = reg.get<TransformComponent>(childHandle);
                        auto& mesh = reg.get<MeshComponent>(childHandle);
                        auto& material = reg.get<MaterialComponent>(childHandle);
                        
                        // Skip if mesh is still loading
                        if (!mesh.isLoading) {
                            RenderCommand command;
                            command.entity = childEntityPtr;
                            command.material = material.material;
                            command.mesh = mesh.mesh;
                            command.transform = transform.transformMatrix();
                            command.isSkeletal = false;
                            
                            queue->add(command);
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
        }
        
        // Step 3: Process regular mesh entities
        {
            RAPTURE_PROFILE_SCOPE("Process Regular Entities");
            auto regularView = reg.view<TransformComponent, MeshComponent, MaterialComponent>(
                entt::exclude<SkeletonComponent, EntityNodeComponent>);
            
            for (auto entityHandle : regularView) {
                // Check for shutdown
                if (s_shuttingDown) {
                    queue->markAsDone();
                    return;
                }
                
                RAPTURE_PROFILE_SCOPE("Process Entity");
                uint32_t entityId = static_cast<uint32_t>(entityHandle);
                
                // Skip if already processed
                if (processedEntities.count(entityId) > 0) {
                    continue;
                }
                
                // Get components directly
                auto& transform = regularView.get<TransformComponent>(entityHandle);
                auto& mesh = regularView.get<MeshComponent>(entityHandle);
                auto& material = regularView.get<MaterialComponent>(entityHandle);
                
                // Skip if mesh is still loading
                if (mesh.isLoading) {
                    continue;
                }
                
                // Create Entity for the command
                auto entitySharedPtr = std::make_shared<Entity>(Entity(entityHandle, scene.get()));
                
                RenderCommand command;
                command.entity = entitySharedPtr;
                command.material = material.material;
                command.mesh = mesh.mesh;
                command.transform = transform.transformMatrix();
                command.isSkeletal = false;
                
                queue->add(command);
            }
        }

        
        
        // Mark the queue as done when finished
        queue->markAsDone();
    }

    RenderQueue CommandQueueBuilder::buildGeometryCommandQueue(const std::shared_ptr<Scene> &scene)
    {
        RAPTURE_PROFILE_FUNCTION();

        RenderQueue queue("GeometryQueue", RenderQueueType::GEOMETRY);
        auto& reg = scene->getRegistry();
        auto& sceneConfig = scene->getSettings();
        
        // Track processed entities to avoid duplicates
        std::unordered_set<uint32_t> processedEntities;
        
        // Step 1: Process skeletal hierarchies first
        // These are entities with both Skeleton and EntityNode components
        auto skeletalView = reg.view<SkeletonComponent, EntityNodeComponent>();
        
        for (auto entityHandle : skeletalView) {
            uint32_t entityId = static_cast<uint32_t>(entityHandle);
            
            // Skip if already processed
            if (processedEntities.count(entityId) > 0) {
                continue;
            }
            
            processedEntities.insert(entityId);
            
            // Get components directly from view to avoid additional registry lookups
            auto& nodeComp = skeletalView.get<EntityNodeComponent>(entityHandle);
            auto& skeletonComp = skeletalView.get<SkeletonComponent>(entityHandle);
            
            // Get animation if available, only create Entity for this specific check
            std::shared_ptr<Animation> animation = nullptr;
            if (reg.all_of<AnimationComponent>(entityHandle)) {
                animation = reg.get<AnimationComponent>(entityHandle).animation;
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
                auto childEntityPtr = node->getEntity();
                if (!childEntityPtr) return;
                
                auto childHandle = static_cast<entt::entity>(*childEntityPtr);
                uint32_t childId = static_cast<uint32_t>(childHandle);
                
                // Skip if already processed
                if (processedEntities.count(childId) > 0) {
                    return;
                }
                
                processedEntities.insert(childId);
                
                // Use direct registry checks instead of entity wrapper methods
                if (reg.all_of<TransformComponent, MeshComponent, MaterialComponent>(childHandle)) {
                    auto& transform = reg.get<TransformComponent>(childHandle);
                    auto& mesh = reg.get<MeshComponent>(childHandle);
                    auto& material = reg.get<MaterialComponent>(childHandle);
                    
                    // Skip if mesh is still loading
                    if (!mesh.isLoading) {
                        RenderCommand command;
                        command.entity = childEntityPtr;
                        command.material = material.material;
                        command.mesh = mesh.mesh;
                        command.transform = transform.transformMatrix();
                        command.isSkeletal = true;
                        
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
        auto hierarchyView = reg.view<EntityNodeComponent>(entt::exclude<SkeletonComponent>);
        
        for (auto entityHandle : hierarchyView) {
            uint32_t entityId = static_cast<uint32_t>(entityHandle);
            
            // Skip if already processed
            if (processedEntities.count(entityId) > 0) {
                continue;
            }
            
            auto& nodeComp = hierarchyView.get<EntityNodeComponent>(entityHandle);
            
            // Process all renderable entities in this hierarchy
            std::function<void(const std::shared_ptr<EntityNode>&)> processNode = 
            [&](const std::shared_ptr<EntityNode>& node) {
                auto childEntityPtr = node->getEntity();
                if (!childEntityPtr) return;
                
                auto childHandle = static_cast<entt::entity>(*childEntityPtr);
                uint32_t childId = static_cast<uint32_t>(childHandle);
                
                // Skip if already processed
                if (processedEntities.count(childId) > 0) {
                    return;
                }
                
                processedEntities.insert(childId);
                
                // Use direct registry checks instead of entity wrapper methods
                if (reg.all_of<TransformComponent, MeshComponent, MaterialComponent>(childHandle)) {
                    auto& transform = reg.get<TransformComponent>(childHandle);
                    auto& mesh = reg.get<MeshComponent>(childHandle);
                    auto& material = reg.get<MaterialComponent>(childHandle);
                    
                    // Skip if mesh is still loading
                    if (!mesh.isLoading) {
                        RenderCommand command;
                        command.entity = childEntityPtr;
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
        // Get components directly from the view to avoid registry lookups
        auto regularView = reg.view<TransformComponent, MeshComponent, MaterialComponent>(
            entt::exclude<SkeletonComponent, EntityNodeComponent>);
        
        for (auto entityHandle : regularView) {
            uint32_t entityId = static_cast<uint32_t>(entityHandle);
            
            // Skip if already processed
            if (processedEntities.count(entityId) > 0) {
                continue;
            }
            
            // Get all required components directly from the view
            auto& transform = regularView.get<TransformComponent>(entityHandle);
            auto& mesh = regularView.get<MeshComponent>(entityHandle);
            auto& material = regularView.get<MaterialComponent>(entityHandle);
            
            // Skip if mesh is still loading
            if (mesh.isLoading) {
                continue;
            }
            
            // Create an Entity only once for the command
            auto entitySharedPtr = std::make_shared<Entity>(Entity(entityHandle, scene.get()));
            
            RenderCommand command;
            command.entity = entitySharedPtr;
            command.material = material.material;
            command.mesh = mesh.mesh;
            command.transform = transform.transformMatrix();
            command.isSkeletal = false;
            
            queue.add(command);
        }
        
        return std::move(queue);
    }

    std::shared_ptr<RenderQueue> CommandQueueBuilder::buildGeometryCommandQueueAsync(const std::shared_ptr<Scene>& scene) 
    {
        RAPTURE_PROFILE_FUNCTION();
        
        // Initialize the system if needed
        if (!s_initialized && !s_shuttingDown) {
            init();
        }
        
        // Skip if we're shutting down or not initialized
        if (s_shuttingDown || !s_initialized) {
            GE_RENDER_WARN("Cannot build async queue - system not initialized or shutting down");
            return nullptr;
        }
        
        // Create the result queue
        auto resultQueue = std::make_shared<RenderQueue>("AsyncGeometryQueue", RenderQueueType::GEOMETRY);
        
        // Create a request
        QueueBuildRequest request;
        request.scene = scene;
        request.resultQueue = resultQueue;
        
        // Queue the request
        {
            std::lock_guard<std::mutex> lock(s_queueMutex);
            s_pendingBuilds.push(request);
        }
        
        // Signal worker threads
        s_queueCV.notify_one();
        
        return resultQueue;
    }

    RenderQueue CommandQueueBuilder::buildPostProcessCommandQueue(const std::shared_ptr<Scene> &scene)
    {
        RAPTURE_PROFILE_FUNCTION();

        RenderQueue queue("PostProcessQueue", RenderQueueType::POSTPROCESS);
        auto& reg = scene->getRegistry();
        auto& sceneConfig = scene->getSettings();

        auto view = reg.view<TransformComponent, MeshComponent, MaterialComponent>();

        for (auto entity_handle : view) {
            RAPTURE_PROFILE_SCOPE("the for loop");
            Entity entity(entity_handle, scene.get());
            auto& transform = view.get<TransformComponent>(entity_handle);
            auto& mesh = view.get<MeshComponent>(entity_handle);
            auto& material = view.get<MaterialComponent>(entity_handle);

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

        return std::move(queue);
    }

    void CommandQueueBuilder::shutdownWorkers()
    {
        RAPTURE_PROFILE_FUNCTION();
        GE_RENDER_INFO("CommandQueueBuilder: Shutting down worker threads");
        
        // Check if we're already shutting down
        if (s_shuttingDown.exchange(true)) {
            GE_RENDER_INFO("CommandQueueBuilder: Already shutting down");
            return;
        }
        
        // Signal all threads to wake up and check the shutdown flag
        s_queueCV.notify_all();
        
        // Join all worker threads
        for (auto& thread : s_workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // Clear thread vector
        s_workerThreads.clear();
        
        // Clear pending builds
        {
            std::lock_guard<std::mutex> lock(s_queueMutex);
            while (!s_pendingBuilds.empty()) {
                auto& request = s_pendingBuilds.front();
                if (request.resultQueue) {
                    request.resultQueue->markAsDone();
                }
                s_pendingBuilds.pop();
            }
        }
        
        // Reset initialized flag
        s_initialized = false;
        
        GE_RENDER_INFO("CommandQueueBuilder: All worker threads shut down");
    }

    void CommandQueueBuilder::processCompletedQueues()
    {
        // This method doesn't need to do anything with our current design
        // because the consumer directly consumes from the queue
        // but could be used for additional processing if needed
    }
}
