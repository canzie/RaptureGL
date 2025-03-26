#pragma once

#include "imgui.h"
#include <array>


class StatsPanel {
public:
    StatsPanel() = default;
    ~StatsPanel() = default;

    void render(float timestep);
};

