#pragma once

#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
#include "../Scenes/Entity.h"
#include "../Materials/Material.h"
#include "../Mesh/Mesh.h"
#include "../Animations/Skeleton/Skeleton.h"
#include "../Animations/Animation.h"

namespace Rapture {

    struct Command {
        uint64_t sortKey = 0;  // Bit-packed sorting key

        // Custom comparison operator for sorting
        bool operator<(const Command& other) const {
            return sortKey < other.sortKey;
        }

    };

    struct RenderCommand : Command {
        std::shared_ptr<Entity> entity = nullptr;
        std::shared_ptr<Material> material = nullptr;
        std::shared_ptr<Mesh> mesh = nullptr;
        glm::mat4 transform;

        bool isSkeletal = false;
        


    };
    
    struct AnimationSetupCommand : Command {
        std::shared_ptr<Skeleton> skeleton = nullptr;
        std::shared_ptr<Animation> animation = nullptr;
    };

    struct PostProcessCommand : Command {
        bool isEnabled;

    };





}