#pragma once

#include <memory>
#include <vector>

#include "Entity.h"


namespace Rapture
{


    // used to create a hierarchy of entities and save the relationships between them
    // usefull for complex meshes with a hierarchy of submeshes and when they each need their own components
    class EntityNode : public std::enable_shared_from_this<EntityNode>
    {
    public:
        EntityNode(std::shared_ptr<Entity> entity);
        EntityNode(std::shared_ptr<Entity> entity, std::shared_ptr<EntityNode> parent);
        ~EntityNode();

        // Setters
        void addChild(std::shared_ptr<EntityNode> child);
        void setParent(std::shared_ptr<EntityNode> parent);
        void removeParent();
        void removeChild(std::shared_ptr<EntityNode> child);
        // Getters
        std::shared_ptr<Entity> getEntity() const;

        std::vector<std::shared_ptr<EntityNode>> getChildren() const;
        std::shared_ptr<EntityNode> getParent() const;

    private:    
        std::shared_ptr<Entity> m_entity;
        std::vector<std::shared_ptr<EntityNode>> m_children;
        std::shared_ptr<EntityNode> m_parent;
    };


    
} // namespace name

