#include "Octree.h"

#include "../Scenes/Components/Components.h"
#include "ColliderPrimitives.h"

namespace Rapture {
namespace Entropy {

    StaticOctree::StaticOctree(std::shared_ptr<Scene> scene)
    {
        buildTree(scene);
    }

    void StaticOctree::buildTree(std::shared_ptr<Scene> scene)
    {

    }

} // namespace Entropy
} // namespace Rapture
