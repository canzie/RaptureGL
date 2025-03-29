// skeleton will


#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <memory>

namespace Rapture
{

    struct Bone : public std::enable_shared_from_this<Bone>
    {
        std::string name;
        glm::mat4 transform;
        glm::mat4 inverseBind;

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

        Skeleton() : m_name("Armature") {}
        Skeleton(const std::string& name) : m_name(name) {}


        // updates the bone and its children with a transform
        void propegateBoneUpdate(Bone& bone, const glm::mat4& transform);

        std::shared_ptr<Bone> getBone(const std::string& name);


        void applyInverseBinds(std::vector<glm::mat4>& inverseBinds);

        void createBones(std::vector<std::string>& boneNames);
        void createBones(std::vector<unsigned int>& boneNames);

        void printHierarchy(std::shared_ptr<Bone> bone=nullptr, std::string indent="");


    private:

        std::string m_name;
        // used to bind them in the correct order
        std::vector<std::shared_ptr<Bone>> m_bones;


    };
}
