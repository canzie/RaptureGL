#pragma once

#include "../Scene.h"
#include "../Components/Components.h"
#include "../../Animations/Animation.h"
#include "../../Logger/Log.h"

namespace Rapture {

    class AnimationSystem {
    public:
        /**
         * @brief Updates all animation components in the scene
         * 
         * @param scene Scene to process
         * @param deltaTime Time elapsed since last frame
         */
        static void updateAnimations(Scene& scene, float deltaTime) {
            auto view = scene.getRegistry().view<AnimationComponent, SkeletonComponent>();
            
            for (auto entity : view) {
                auto& animComp = view.get<AnimationComponent>(entity);
                auto& skelComp = view.get<SkeletonComponent>(entity);
                
                // Update animation time
                animComp.update(deltaTime);
                
                // Apply animation to skeleton
                if (animComp.animation && animComp.animation->isPlaying()) {
                    animComp.applyToSkeleton(skelComp.skeleton);
                }
            }
        }
        
        /**
         * @brief Start playing animation on a specific entity
         * 
         * @param entity Entity to play animation on
         * @param animationIndex Index of animation to play (default: 0)
         * @return true if successful, false otherwise
         */
        static bool playAnimation(Entity entity, int animationIndex = 0) {
            if (!entity.hasComponent<AnimationComponent>()) {
                return false;
            }
            
            auto& animComp = entity.getComponent<AnimationComponent>();
            if (animationIndex >= 0 && animationIndex < animComp.animations.size()) {
                animComp.setAnimation(animationIndex);
                animComp.playAnimation();
                return true;
            }
            
            return false;
        }
        
        /**
         * @brief Pause animation on a specific entity
         * 
         * @param entity Entity to pause animation on
         * @return true if successful, false otherwise
         */
        static bool pauseAnimation(Entity entity) {
            if (!entity.hasComponent<AnimationComponent>()) {
                return false;
            }
            
            auto& animComp = entity.getComponent<AnimationComponent>();
            animComp.pauseAnimation();
            return true;
        }
        
        /**
         * @brief Stop animation on a specific entity
         * 
         * @param entity Entity to stop animation on
         * @return true if successful, false otherwise
         */
        static bool stopAnimation(Entity entity) {
            if (!entity.hasComponent<AnimationComponent>()) {
                return false;
            }
            
            auto& animComp = entity.getComponent<AnimationComponent>();
            animComp.stopAnimation();
            return true;
        }
        
        /**
         * @brief Reset animation on a specific entity
         * 
         * @param entity Entity to reset animation on
         * @return true if successful, false otherwise
         */
        static bool resetAnimation(Entity entity) {
            if (!entity.hasComponent<AnimationComponent>()) {
                return false;
            }
            
            auto& animComp = entity.getComponent<AnimationComponent>();
            animComp.resetAnimation();
            return true;
        }
    };

} 