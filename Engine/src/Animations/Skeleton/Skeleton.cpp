#include "Skeleton.h"

#include "../../Logger/Log.h"
#include "../../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"

namespace Rapture {
    Skeleton::Skeleton(const std::string &name)
    : m_name(name)
    {        
        m_boneMatricesUBO = std::make_shared<UniformBuffer>(
            sizeof(m_boneMatricesData), 
            BufferUsage::Dynamic, 
            &m_boneMatricesData, 
            BONE_MATRICES_BINDING_POINT_IDX);

        m_rootBone = std::make_shared<Bone>();
        m_rootBone->name = "RootBone";


    }

    void Skeleton::propegateBoneUpdate(std::shared_ptr<Bone> bone, const glm::mat4 &parentWorldTransform)
    {
        // Calculate world transform for this bone by combining parent's world transform with 
        // bone's local transform
        glm::mat4 worldTransform = parentWorldTransform * bone->transform;
        
        // Store the world transform (or use it for rendering)
        // Note: typically you'd store local transforms separately from world transforms
        //glm::mat4 localTransform = bone->transform;  // Save local transform
        bone->worldTransform = worldTransform;  // Store world transform for rendering
        
        // Propagate to children (each child's transform is in LOCAL space relative to this bone)
        for (auto& child : bone->children) {
            propegateBoneUpdate(child, worldTransform);
        }
        
        // Mark bones as needing update in shader
        m_isBoneDirty = true;

    }

    std::shared_ptr<Bone> Skeleton::getBone(const std::string &name)
    {
        for (auto& bone : m_bones) {
            if (bone->name == name) {
                return bone;
            }
        }
        return nullptr;
    }


    void Skeleton::applyInverseBinds(std::vector<glm::mat4> &inverseBinds)
    {
        int i = 0;  
        for (auto& bone : m_bones) {
            bone->inverseBind = inverseBinds[i];
            m_boneMatricesData.u_BoneTransforms[i] = inverseBinds[i];
            i++;
        }

        propegateBoneUpdate(m_bones[0], glm::mat4(1.0f));

    }

    void Skeleton::createBones(std::vector<std::string> &boneNames)
    {
        for (auto& boneName : boneNames) {
            auto bone = std::make_shared<Bone>();
            bone->name = boneName;
            m_bones.push_back(bone);
        }
    }

    // for ease of use, can just use the indices here
    void Skeleton::createBones(std::vector<unsigned int> &boneIndices)
    {
        for (auto& boneIndex : boneIndices) {
            auto bone = std::make_shared<Bone>();
            bone->name = std::to_string(boneIndex);
            m_bones.push_back(bone);
        }
    }

    void Skeleton::printHierarchy(std::shared_ptr<Bone> bone, std::string indent)
    {
        if (bone == nullptr) {
            bone = m_bones[0];
        }

        GE_CORE_TRACE("{}Bone_{}", indent, bone->name);
        for (auto& child : bone->children) {
            printHierarchy(child, indent + "  ");
        }
    }

    void Skeleton::bindBones()
    {

        m_boneMatricesUBO->bindBase(BONE_MATRICES_BINDING_POINT_IDX);

        if (m_isBoneDirty) {
            int i = 0;
            for (auto& bone : m_bones) {
                m_boneMatricesData.u_BoneTransforms[i] = bone->worldTransform * bone->inverseBind;
                i++;
            }


        
            m_boneMatricesUBO->setData(&m_boneMatricesData, sizeof(m_boneMatricesData));
            m_boneMatricesUBO->flush();

            m_isBoneDirty = false;
        }

    }
}
