#include "Animation.h"
#include "../File Loaders/glTF/glTF2Loader.h"

#include <string>
#include <vector>
#include <memory>
#include <iostream>

// Example usage of the animation system
namespace AnimationSystemExample {

    void runAnimationExample() {
        // Step 1: Create a scene (for glTF loading)
        auto scene = std::make_shared<Rapture::Scene>();
        
        // Step 2: Create a glTF loader
        Rapture::glTF2Loader loader(scene);
        
        // Step 3: Load animations from a glTF file
        std::string filepath = "assets/models/character.gltf";
        std::vector<std::shared_ptr<Rapture::Animation>> animations = loader.loadAnimations(filepath);
        
        if (animations.empty()) {
            std::cout << "No animations found in the file" << std::endl;
            return;
        }
        
        // Step 4: Get animation info
        std::cout << "Loaded " << animations.size() << " animations:" << std::endl;
        
        for (const auto& animation : animations) {
            std::cout << "Animation: " << animation->getName() 
                << " (Duration: " << animation->getDuration() << "s)" << std::endl;
        }
        
        // Step 5: Set up animation control
        auto walkAnimation = animations[0]; // Assuming first animation is a walking animation
        walkAnimation->setLooping(true);
        walkAnimation->setPlaybackSpeed(1.0f);
        
        // Step 6: Create a skeleton to apply the animation to
        auto skeleton = std::make_shared<Rapture::Skeleton>("CharacterSkeleton");
        
        // In a real application, the skeleton would be loaded from the glTF file
        // Here we assume it's already loaded
        
        // Step 7: Play the animation
        walkAnimation->play();
        
        // Step 8: Create an animation blend
        Rapture::AnimationBlender blender;
        
        // Add animations with weights
        blender.addAnimation(walkAnimation, 0.7f);
        
        if (animations.size() > 1) {
            auto runAnimation = animations[1]; // Assuming second animation is a running animation
            runAnimation->play();
            blender.addAnimation(runAnimation, 0.3f);
        }
        
        // Step 9: Update and apply animations in game loop
        float deltaTime = 0.016f; // ~60 FPS
        
        // This would be called every frame in the game loop
        for (int frame = 0; frame < 100; frame++) {
            // Update all animations
            for (auto& anim : animations) {
                if (anim->isPlaying()) {
                    anim->update(deltaTime);
                }
            }
            
            // Option 1: Apply a single animation directly
            // walkAnimation->applyToSkeleton(skeleton);
            
            // Option 2: Apply blended animations
            blender.apply(skeleton);
            
            // Bind the skeleton for rendering (this updates the UBO for the shader)
            skeleton->bindBones();
            
            // Simulate frame time
            std::cout << "Frame " << frame << ": Animation time " 
                << walkAnimation->getCurrentTime() << "s" << std::endl;
            
            // In a real application, you would render the scene here
        }
        
        // Step 10: Animation control
        walkAnimation->pause();
        walkAnimation->reset();
        walkAnimation->play();
        
        // Modify blending weights at runtime
        blender.setAnimationWeight(walkAnimation->getName(), 0.2f);
        if (animations.size() > 1) {
            blender.setAnimationWeight(animations[1]->getName(), 0.8f);
        }
        
        // Animation Manager usage
        auto& animManager = Rapture::AnimationManager::getInstance();
        
        // Register animations with the manager
        for (auto& anim : animations) {
            animManager.addAnimation(anim);
        }
        
        // Get an animation by name later
        auto retrievedAnim = animManager.getAnimation(walkAnimation->getName());
        if (retrievedAnim) {
            retrievedAnim->setPlaybackSpeed(2.0f); // Double speed
        }
        
        // Update all active animations in one call (usually in main game loop)
        animManager.update(deltaTime);
    }
} 