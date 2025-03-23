#include "EntityNode.h"



namespace Rapture
{

    EntityNode::EntityNode(std::shared_ptr<Entity> entity)
        : m_entity(entity) { }

    EntityNode::EntityNode(std::shared_ptr<Entity> entity, std::shared_ptr<EntityNode> parent)
        : m_entity(entity), m_parent(parent) { }

    EntityNode::~EntityNode() {

        m_children.clear();
        m_parent = nullptr;


        if (m_parent != nullptr) {

            for (auto child : m_children)
            {
                child->setParent(m_parent);
                m_parent->addChild(child);
            }

        } else {
            for (auto child : m_children)
            {
                child->setParent(nullptr);
            }
        }

    }

    // Add node to children
    // If child already has a parent, remove it from that parent first
    // Then set this node as the new parent
    void EntityNode::addChild(std::shared_ptr<EntityNode> child) {

        // Check if child already has a parent
        if (auto existingParent = child->getParent())
        {
            existingParent->removeChild(child);
        }
        
        // Set this as the child's parent
        child->setParent(shared_from_this());
        
        // Add to children collection
        m_children.push_back(child);
    }

    void EntityNode::removeParent()
    {
        setParent(nullptr);
    }

    void EntityNode::removeChild(std::shared_ptr<EntityNode> child)
    {

        m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
        child->setParent(nullptr);
    }



    void EntityNode::setParent(std::shared_ptr<EntityNode> parent)
    {
        m_parent = parent;
    }

    std::shared_ptr<Entity> EntityNode::getEntity() const
    {
        return m_entity;
    }

    std::vector<std::shared_ptr<EntityNode>> EntityNode::getChildren() const
    {   
        return m_children;
    }

    std::shared_ptr<EntityNode> EntityNode::getParent() const
    {
        return m_parent;
    }


} // namespace Rapture

