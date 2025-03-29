#include "Skeleton.h"

#include "../../Logger/Log.h"

namespace Rapture {
    void Skeleton::propegateBoneUpdate(Bone &bone, const glm::mat4 &transform)
    {
        GE_CORE_ERROR("Skeleton::propegateBoneUpdate not implemented yet in Skeleton.cpp");
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
            bone->inverseBind = inverseBinds[i++];
            
        }
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
    
}
