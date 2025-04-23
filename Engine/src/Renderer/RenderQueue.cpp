#include "RenderQueue.h"
#include "../Scenes/Components/Components.h"
#include <unordered_set>
#include <unordered_map> // Needed for grouping

#include "RadianceCascades/RadianceCascades.h"

namespace Rapture {

    // Initialize static members
    std::atomic_bool CommandQueueBuilder::s_initialized(false);
    std::atomic_bool CommandQueueBuilder::s_shuttingDown(false);
    std::vector<std::thread> CommandQueueBuilder::s_workerThreads;
    std::mutex CommandQueueBuilder::s_queueMutex;
    std::condition_variable CommandQueueBuilder::s_queueCV;
    std::queue<QueueBuildRequest> CommandQueueBuilder::s_pendingBuilds;

    // Helper function to check if an entity or any of its ancestors has a skeleton - DEPRECATED / No longer needed?
    /*
    static bool hasSkeletonInHierarchy(const std::shared_ptr<EntityNode>& node) {
        RAPTURE_PROFILE_SCOPE("hasSkeletonInHierarchy");
        // ... implementation ... // This function relied on recursive traversal, which we are removing.
        return false; // Placeholder
    }
    */
    
    // Helper function to find the skeleton & animation at the root of a hierarchy - DEPRECATED / No longer needed?
    /*
    static std::pair<std::shared_ptr<Skeleton>, std::shared_ptr<Animation>> 
    findSkeletonInHierarchy(const std::shared_ptr<EntityNode>& node) {
         // ... implementation ... // This function relied on recursive traversal, which we are removing.
        return {nullptr, nullptr}; // Placeholder
    }
    */

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



    void CommandQueueBuilder::shutdownWorkers()
    {
        RAPTURE_PROFILE_FUNCTION();
        GE_RENDER_INFO("CommandQueueBuilder: Shutting down worker threads");
        // Use exchange to prevent race conditions on shutdown flag
        if (s_shuttingDown.exchange(true)) {
             GE_RENDER_INFO("CommandQueueBuilder: Already shutting down");
             return; // Already shutting down or shut down
        }

        // Signal all threads to wake up and check the shutdown flag
        s_queueCV.notify_all();

        // Join all worker threads
        GE_RENDER_INFO("CommandQueueBuilder: Waiting for worker threads to join");
        for (auto& thread : s_workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        s_workerThreads.clear(); // Clear the thread vector after joining
        GE_RENDER_INFO("CommandQueueBuilder: Worker threads joined");

        // Clear pending builds queue under lock
        {
            std::lock_guard<std::mutex> lock(s_queueMutex);
             GE_RENDER_INFO("CommandQueueBuilder: Clearing pending build requests. Count: {}", s_pendingBuilds.size());
            while (!s_pendingBuilds.empty()) {
                auto& request = s_pendingBuilds.front();
                if (request.resultQueue) {
                     request.resultQueue->markAsDone(); // Mark associated queues as done/cancelled
                }
                s_pendingBuilds.pop();
            }
        }
        
        // Reset initialized flag AFTER everything is cleaned up
        s_initialized = false;
        
        GE_RENDER_INFO("CommandQueueBuilder: Shutdown complete");
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
                    // Wait if queue is empty AND we are not shutting down
                    return !s_pendingBuilds.empty() || s_shuttingDown.load();
                });
                
                // Check if we should exit AFTER waking up
                if (s_shuttingDown.load() && s_pendingBuilds.empty()) {
                     // Exit only if shutting down AND queue is empty
                     break;
                }
                
                // Get next request if available
                if (!s_pendingBuilds.empty()) {
                    request = std::move(s_pendingBuilds.front()); // Use move
                    s_pendingBuilds.pop();
                    hasRequest = true;
                } else {
                     // Spurious wake-up or shutdown signal without work, continue waiting
                     continue;
                }
            } // Lock released here
            
            // Process request outside of lock
            if (hasRequest && request.scene && request.resultQueue) {
                RAPTURE_PROFILE_SCOPE("Process Queue Build Request");
                
                try {
                    // Build geometry queue based on type
                    // Use a switch for clarity
                    switch (request.resultQueue->m_type) {
                        case RenderQueueType::FORWARD:
                            buildForwardQueue(request); // Pass true for isFinal
                            break;
                        case RenderQueueType::DEFERRED:
                            buildDeferredQueue(request);
                            break;
                        case RenderQueueType::SHADOWMAP:
                            buildShadowPassQueue(request);
                            break;
                        case RenderQueueType::POSTPROCESS:
                             // Assuming buildPostProcessCommandQueue might be used elsewhere or integrated differently
                             GE_RENDER_WARN("Async build requested for PostProcess queue, not fully supported yet.");
                             request.resultQueue->markAsDone(); // Mark done immediately if not handled
                             break;
                        case RenderQueueType::RADIANCE_CASCADES:
                            buildRadianceCascadesQueue(request);
                            break;
                         default:
                              GE_RENDER_ERROR("Unknown RenderQueueType requested for async build.");
                              request.resultQueue->markAsDone();
                              break;
                    }

                }
                catch (const std::exception& e) {
                    GE_RENDER_ERROR("Exception in queue builder thread: {}", e.what());
                    // Ensure queue is marked done even on exception
                    if (request.resultQueue) request.resultQueue->markAsDone();
                }
                catch (...) {
                    GE_RENDER_ERROR("Unknown exception in queue builder thread");
                     // Ensure queue is marked done even on exception
                    if (request.resultQueue) request.resultQueue->markAsDone();
                }
            } else if (hasRequest) {
                 // Handle cases where request might be invalid (e.g., null scene/queue)
                 GE_RENDER_WARN("CommandQueueBuilder: Worker thread received invalid request.");
                 // Make sure to mark queue done if it exists
                 if(request.resultQueue) request.resultQueue->markAsDone();
            }
        } // End of while loop
        
        GE_RENDER_INFO("CommandQueueBuilder: Worker thread stopped");
    }

    void CommandQueueBuilder::buildForwardQueue(const QueueBuildRequest& request)
    {
        RAPTURE_PROFILE_FUNCTION();

        GeometryQueueBuilderConfig config;
        buildGeometryQueue(request, config);
        request.resultQueue->markAsDone();

    }

    // Refactored buildGeometryQueue using SkeletonRefComponent
    void CommandQueueBuilder::buildGeometryQueue(const QueueBuildRequest& request, GeometryQueueBuilderConfig config)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        auto& scene = request.scene;
        auto& queue = request.resultQueue;
        auto& reg = scene->getRegistry();

        uint32_t culledEntities = 0;

        // --- Temporary Storage ---
        // Map: Skeleton Ptr -> Vector of RenderCommands associated with that skeleton
        std::unordered_map<std::shared_ptr<Skeleton>, std::vector<RenderCommand>> skeletalMeshCommands;
        // Vector: RenderCommands for non-skeletal meshes
        // std::vector<RenderCommand> nonSkeletalMeshCommands;
        // Map: Skeleton Ptr -> Animation Ptr (for AnimationSetupCommand)
        std::unordered_map<std::shared_ptr<Skeleton>, std::shared_ptr<Animation>> skeletonAnimations;

        // --- Pass 1: Collect Animation Info for Skeleton Roots ---
        {
            RAPTURE_PROFILE_SCOPE("Pass 1: Collect Animation Info");
            // View entities that HAVE a SkeletonComponent (roots) and might have AnimationComponent
            auto skeletonRootView = reg.view<SkeletonComponent>(); // No need for AnimationComponent here yet
            
            for (auto entityHandle : skeletonRootView) {
                 // Check for shutdown condition periodically
                if (s_shuttingDown) { queue->markAsDone(); return; }

                auto& skeletonComp = skeletonRootView.get<SkeletonComponent>(entityHandle);
                if (!skeletonComp.skeleton) continue; // Skip if skeleton ptr is null

                // Check if this skeleton root ALSO has an AnimationComponent
                std::shared_ptr<Animation> animation = nullptr;
                if (auto* animComp = reg.try_get<AnimationComponent>(entityHandle)) {
                    animation = animComp->animation; // Get the currently active animation
                }
                
                // Store the skeleton and its potential animation
                skeletonAnimations[skeletonComp.skeleton] = animation;
            }
        }

        // --- Pass 2: Collect Render Commands (Skeletal and Non-Skeletal) ---
        {
            RAPTURE_PROFILE_SCOPE("Pass 2: Collect Render Commands");
            // View all entities that are renderable (have Transform, Mesh, Material)
            auto renderableView = reg.view<TransformComponent, MeshComponent, MaterialComponent>();
            auto boundingBoxes = reg.view<BoundingBoxComponent>();

            for (auto entityHandle : renderableView) {
                 // Check for shutdown condition periodically
                if (s_shuttingDown) { queue->markAsDone(); return; }

                // Get necessary components directly from the view
                auto& transformComp = renderableView.get<TransformComponent>(entityHandle);
                auto& meshComp = renderableView.get<MeshComponent>(entityHandle);
                auto& materialComp = renderableView.get<MaterialComponent>(entityHandle);

                // Skip if mesh is still loading or invalid
                if (meshComp.isLoading || !meshComp.mesh || !materialComp.material) {
                    continue;
                }

                
                if (config.frustum && boundingBoxes.contains(entityHandle)) {
                    auto& boundingBoxComp = boundingBoxes.get<BoundingBoxComponent>(entityHandle);
                    FrustumResult result = config.frustum->testBoundingBox(boundingBoxComp.worldBoundingBox);
                    if (result == FrustumResult::Outside) {
                        culledEntities++;
                        continue;
                        
                    }
                }
                

                // Create the base RenderCommand
                RenderCommand command;
                // Optimization: Avoid creating shared_ptr<Entity> for every command if possible.
                // Consider passing entt::entity handle and Scene* if renderer can use them.
                // For now, keeping std::shared_ptr<Entity> as per original structure.
                command.entity = std::make_shared<Entity>(Entity(entityHandle, scene.get())); 
                command.material = materialComp.material;
                command.mesh = meshComp.mesh;
                command.transform = transformComp.transformMatrix();
                command.isSkeletal = false; // Default to false

                bool addedToSkeletal = false;

                // Check for SkeletonRefComponent (child of a skeleton)
                if (auto* skelRefComp = reg.try_get<SkeletonRefComponent>(entityHandle)) {
                    if (auto lockedSkeleton = skelRefComp->skeleton.lock()) {
                        command.isSkeletal = true;
                        skeletalMeshCommands[lockedSkeleton].push_back(command);
                        addedToSkeletal = true;
                    } else {
                         // Log warning: SkeletonRefComponent exists but weak_ptr expired?
                         GE_CORE_WARN("CommandQueueBuilder::BuildGeometryQueue - Entity {} has SkeletonRefComponent but the weak_ptr is expired.", (uint32_t)entityHandle);
                    }
                }

                // If not added via SkeletonRef, check if it's the Skeleton root itself
                if (!addedToSkeletal) {
                     if (auto* skelComp = reg.try_get<SkeletonComponent>(entityHandle)) {
                         if (skelComp->skeleton){ // Check if skeleton pointer is valid
                              command.isSkeletal = true;
                              skeletalMeshCommands[skelComp->skeleton].push_back(command);
                              addedToSkeletal = true;
                         }
                     }
                }

                // If not added to skeletal map, add to non-skeletal list -> ADD DIRECTLY TO QUEUE
                if (!addedToSkeletal) {
                    queue->add(command); // ADDED: Add non-skeletal commands directly
                }
            }
        }

        // --- Pass 3: Build Final Queue ---
        {
            RAPTURE_PROFILE_SCOPE("Pass 3: Build Final Queue");
            
            // Add skeletal commands, grouped by skeleton
            for (auto const& [skeletonPtr, commands] : skeletalMeshCommands) {
                 // Check for shutdown condition periodically
                 if (s_shuttingDown) { queue->markAsDone(); return; }

                // Add AnimationSetupCommand for this skeleton
                AnimationSetupCommand setupCmd;
                setupCmd.skeleton = skeletonPtr;
                // Find animation from the map collected in Pass 1
                auto animIt = skeletonAnimations.find(skeletonPtr);
                if (animIt != skeletonAnimations.end()) {
                    setupCmd.animation = animIt->second;
                } else {
                     // Should not happen if Pass 1 collected all SkeletonComponent skeletons
                     // but handle defensively.
                     setupCmd.animation = nullptr;
                }
                queue->add(setupCmd);

                // Add all RenderCommands associated with this skeleton
                for (const auto& renderCmd : commands) {
                     // Check for shutdown condition periodically
                     if (s_shuttingDown) { queue->markAsDone(); return; }
                     queue->add(renderCmd);
                }
            }
        }


        //GE_RENDER_INFO("CommandQueueBuilder::BuildGeometryQueue - Culled {} entities", culledEntities);
    }

    // buildDeferredQueue now uses the refactored buildGeometryQueue
    void CommandQueueBuilder::buildDeferredQueue(const QueueBuildRequest &request)
    {
        RAPTURE_PROFILE_FUNCTION();

        // setup the configuration for specifics like the frustum to use or, 
        // any other render pass specific things
        GeometryQueueBuilderConfig config;
        auto mainCamera = request.scene->getMainCamera();
        if (mainCamera && request.scene->getSettings().frustumCullingEnabled) {
            auto* camComp = mainCamera->tryGetComponent<CameraControllerComponent>();
            if (camComp) {
                config.frustum = std::make_shared<Frustum>(camComp->frustum);
            }
        }

        buildGeometryQueue(request, config); 
        if (s_shuttingDown) { request.resultQueue->markAsDone(); return; } // Check shutdown after buildGeometryQueue


        // Lighting Pass Command
        LightingPassCommand lightingPassCmd;
        // Configuration for lighting pass (e.g., shader handle) would happen here or in the renderer
        request.resultQueue->add(lightingPassCmd);
        if (s_shuttingDown) { request.resultQueue->markAsDone(); return; }

        // SSR Command (Example)
        SSRCommand ssrCmd;
        // Configuration for SSR pass
        request.resultQueue->add(ssrCmd); 
        if (s_shuttingDown) { request.resultQueue->markAsDone(); return; }

        // --- Mark queue as done ---
        request.resultQueue->markAsDone();
    }

    // buildShadowPassQueue now uses the refactored buildGeometryQueue
    void CommandQueueBuilder::buildShadowPassQueue(const QueueBuildRequest &request)
    {
        RAPTURE_PROFILE_FUNCTION();

        auto& scene = request.scene;
        auto& reg = scene->getRegistry();
        auto& queue = request.resultQueue;

        // Get views for each relevant component
        auto lightView = reg.view<LightComponent, TransformComponent>();
        auto shadowView = reg.view<ShadowComponent>();
        auto csmView = reg.view<CascadedShadowComponent>();

        ShadowVariant shadowMap = std::monostate();

        for (auto entityHandle : lightView) {
            GeometryQueueBuilderConfig config;

            // get the shadowmap type/variant
            auto& lightComp = lightView.get<LightComponent>(entityHandle);
            if (lightComp.castsShadow && csmView.contains(entityHandle)) {
                // add cascaded shadow maps
                auto& csmComp = csmView.get<CascadedShadowComponent>(entityHandle);
                if (csmComp.isActive) {
                    shadowMap = csmComp.cascadedShadowMapping;
                    //config.frustum = csmComp.frustum;
                }
            } else if (lightComp.castsShadow && shadowView.contains(entityHandle)) {
                // add regular shadow maps
                auto& shadowComp = shadowView.get<ShadowComponent>(entityHandle);
                if (shadowComp.isActive) {
                    // light frustum is bugged for now
                    config.frustum = shadowComp.frustum;
                    shadowMap = shadowComp.shadowMap;
                    
                }
            }

             
            if (!std::holds_alternative<std::monostate>(shadowMap)) {
                // setup start of 1 shadowpass
                ShadowPassCommand shadowPassStartCmd;
                shadowPassStartCmd.shadowMap = shadowMap;
                shadowPassStartCmd.lightType = lightComp.type;
                shadowPassStartCmd.commandType = CommandExectionPhase::BEGIN_PASS;
                queue->add(shadowPassStartCmd);

                // add the geometryqueue
                buildGeometryQueue(request, config);

                // notify end of the this shadowpass
                ShadowPassCommand shadowPassEndCmd;
                shadowPassEndCmd.shadowMap = shadowMap;
                shadowPassEndCmd.lightType = lightComp.type;
                shadowPassEndCmd.commandType = CommandExectionPhase::END_PASS;
                queue->add(shadowPassEndCmd);

                if (s_shuttingDown) { request.resultQueue->markAsDone(); return; }
            }
        }

        // Mark queue as done
        request.resultQueue->markAsDone();
    }

    void CommandQueueBuilder::buildRadianceCascadesQueue(const QueueBuildRequest &request)
    {
        RAPTURE_PROFILE_FUNCTION();

        // Get the scene and registry
        //auto& scene = request.scene;
        //auto& reg = scene->getRegistry();


        RadianceCascadesCommand radianceCascadesCmd;
        radianceCascadesCmd.cascadeSSBO = RadianceCascadesManager::getSSBO();
        radianceCascadesCmd.radianceCascadesShader = RadianceCascadesManager::getComputeShader();
        radianceCascadesCmd.cascadeHierarchy = RadianceCascadesManager::getHierarchy();




        if (!radianceCascadesCmd.cascadeSSBO || !radianceCascadesCmd.radianceCascadesShader) {
            GE_RENDER_ERROR("RadianceCascades: Error retrieving cascadeSSBO or radianceCascadesShader, Probably not initialized");
            request.resultQueue->markAsDone();
            return;
        }

        request.resultQueue->add(radianceCascadesCmd);


        // Indirect Lighting Pass Command
        IndirectLightingPassCommand indirectLightingPassCmd;
        indirectLightingPassCmd.cascadeSSBO = RadianceCascadesManager::getSSBO();
        indirectLightingPassCmd.cascadeHierarchy = RadianceCascadesManager::getHierarchy();
        request.resultQueue->add(indirectLightingPassCmd);


        request.resultQueue->markAsDone();

    }

    std::shared_ptr<RenderQueue> CommandQueueBuilder::buildGeometryCommandQueueAsync(const std::shared_ptr<Scene>& scene, RenderQueueType type) 
    {
        RAPTURE_PROFILE_FUNCTION();
        
        // Initialize the system if needed (thread-safe check)
        if (!s_initialized.load(std::memory_order_acquire) && !s_shuttingDown.load(std::memory_order_acquire)) {
             // Double-checked locking pattern (optional, simple init might be fine)
             std::lock_guard<std::mutex> lock(s_queueMutex); // Use the queue mutex for synchronization
             if (!s_initialized.load() && !s_shuttingDown.load()) {
                 init(); // Initialize workers if not already done
             }
        }
        
        // Skip if we're shutting down or initialization failed
        if (s_shuttingDown.load(std::memory_order_acquire) || !s_initialized.load(std::memory_order_acquire)) {
            GE_RENDER_WARN("Cannot build async queue - system not initialized or shutting down");
            // Return a completed empty queue instead of nullptr to avoid crashes downstream
             auto emptyQueue = std::make_shared<RenderQueue>("EmptyQueue_Shutdown", type);
             emptyQueue->markAsDone();
             return emptyQueue;
        }
        
        // Create the result queue (make_shared is generally preferred)
        auto resultQueue = std::make_shared<RenderQueue>("AsyncQueue_" + scene->getSceneName(), type);
        
        // Create a request
        QueueBuildRequest request;
        request.scene = scene;
        request.resultQueue = resultQueue;
        
        // Queue the request under lock
        {
            std::lock_guard<std::mutex> lock(s_queueMutex);
            s_pendingBuilds.push(std::move(request)); // Use move
        }
        
        // Signal ONE worker thread
        s_queueCV.notify_one();
        
        return resultQueue; // Return the queue immediately (it will be filled by worker)
    }



    void CommandQueueBuilder::processCompletedQueues()
    {
        // This method remains largely unused if consumers directly check `isDone()`
        // on the shared_ptr<RenderQueue> they receive from the async build call.
        // Could be used for logging, cleanup, or chaining if needed later.
    }
}
