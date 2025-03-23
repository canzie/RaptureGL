#pragma once

#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "Scenes/Components/Components.h"
#include "TestLayer.h"

#include <functional>
#include <vector>
#include <string>

#include "imgui.h"

namespace Rapture {

class EntityBrowserPanel {
public:
    EntityBrowserPanel() = default;
    ~EntityBrowserPanel() = default;

    void render(TestLayer* testLayer);

private:
    // Helper function to display an entity and its children recursively
    void displayEntityHierarchy(entt::entity entityHandle, int depth, Scene* scene);
};

}  // namespace Rapture 