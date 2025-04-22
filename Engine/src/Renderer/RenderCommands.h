#pragma once


#include "../Scenes/Entity.h"
#include "../Materials/Material.h"
#include "../Mesh/Mesh.h"
#include "../Animations/Skeleton/Skeleton.h"
#include "../Animations/Animation.h"
#include "ShadowMapping/ShadowMapping.h"
#include "ShadowMapping/CascadedShadowMapping.h"
#include "../Scenes/Components/Components.h"
#include "../Textures/Texture.h"
#include "../Buffers/OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h"
#include "RadianceCascades/RadianceCascades.h"
#include "RadianceCascades/RadianceCascadesManager.h"

#include <glm/glm.hpp>

#include <variant>
#include <cstdint>
#include <memory>

namespace Rapture {


    enum class CommandExectionPhase {
        NONE,
        BEGIN_PASS, 
        END_PASS   
    };

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

    // Add new command types for deferred rendering
    struct GeometryPassCommand : Command {
        std::shared_ptr<Entity> entity = nullptr;
        std::shared_ptr<Material> material = nullptr;
        std::shared_ptr<Mesh> mesh = nullptr;
        glm::mat4 transform;
        bool isSkeletal = false;
    };
    
    // NOTE: should probably change to be the shader aset handle, becasue if the shader becomes invalid before it is bound, this will cause issues
    struct LightingPassCommand : Command {
        //std::shared_ptr<Shader> lightPassShader = nullptr;
        bool dummy = true;

    };

    struct RadianceCascadesCommand : Command {
        std::shared_ptr<Shader> radianceCascadesShader = nullptr;
        std::shared_ptr<ShaderStorageBuffer> cascadeSSBO = nullptr;
        std::shared_ptr<RadianceCascadeHierarchy> cascadeHierarchy = nullptr;
    };

    struct IndirectLightingPassCommand : Command {
        std::shared_ptr<ShaderStorageBuffer> cascadeSSBO = nullptr;
        std::shared_ptr<RadianceCascadeHierarchy> cascadeHierarchy = nullptr;
    };

    struct SSRCommand : Command {
        std::shared_ptr<Shader> ssrShader = nullptr; 
    };

    using ShadowVariant = std::variant<std::monostate, std::shared_ptr<ShadowMap>, std::shared_ptr<CascadedShadowMapping>>;


    struct ShadowPassCommand : Command {
        CommandExectionPhase commandType = CommandExectionPhase::NONE;
        ShadowVariant shadowMap = std::monostate();
        
        LightType lightType = LightType::Directional; 
    };




}