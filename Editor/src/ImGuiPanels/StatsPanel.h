#pragma once

#include "imgui.h"
#include <array>

namespace Rapture {

class StatsPanel {
public:
    StatsPanel() = default;
    ~StatsPanel() = default;

    void render(float timestep);
};

}  // namespace Rapture 