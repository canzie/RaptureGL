// skeleton will

#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <memory>

#include "../../Materials/MaterialUniformLayouts.h"
#include "../../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"

namespace Rapture
{

    struct Bone : public std::enable_shared_from_this<Bone>
    {
        std::string name = "Bone";
        glm::mat4 transform = glm::mat4(1.0f);
        glm::mat4 worldTransform = glm::mat4(1.0f);
        glm::mat4 inverseBind = glm::mat4(1.0f);

        std::shared_ptr<Bone> parent;
        std::vector<std::shared_ptr<Bone>> children;

        void addChild(std::shared_ptr<Bone> child) {
            children.push_back(child);
            child->parent = shared_from_this();
        }
    };

    class Skeleton
    {

    public:

        Skeleton() : Skeleton("Armature") {}
        Skeleton(const std::string& name);


        // updates the bone and its children with a transform
        void propegateBoneUpdate(std::shared_ptr<Bone> bone, const glm::mat4& transform);

        std::shared_ptr<Bone> getBone(const std::string& name);


        void applyInverseBinds(std::vector<glm::mat4>& inverseBinds);

        void createBones(std::vector<std::string>& boneNames);
        void createBones(std::vector<unsigned int>& boneNames);

        void printHierarchy(std::shared_ptr<Bone> bone=nullptr, std::string indent="");

        // Get all bone matrices in the order they are stored in m_bones
        std::vector<glm::mat4> getBoneMatrices() const {
            std::vector<glm::mat4> matrices;
            matrices.reserve(m_bones.size());
            
            for (const auto& bone : m_bones) {
                matrices.push_back(bone->inverseBind * bone->transform);
            }
            
            return matrices;
        }
        
        // Get the bones vector
        const std::vector<std::shared_ptr<Bone>>& getBones() const {
            return m_bones;
        }

        void bindBones();

        void setRootBoneTransform(const glm::mat4& transform) {
            m_rootBone->transform = transform;
        }

        std::shared_ptr<Bone> getRootBone() const {
            return m_rootBone;
        }

        const std::string& getSkeletonName() const {
            return m_name;
        }



    private:

        std::string m_name;
        // used to bind them in the correct order
        std::vector<std::shared_ptr<Bone>> m_bones;

        std::shared_ptr<Bone> m_rootBone;

        bool m_isBoneDirty = true;
        std::shared_ptr<UniformBuffer> m_boneMatricesUBO;
        BoneMatricesUniform m_boneMatricesData;


    };
}