#include "StatsPanel.h"
#include "Debug/Profiler.h"
#include <array>

namespace Rapture {

void StatsPanel::render(float timestep) {
    ImGui::Begin("Statistics");
    
    // Basic stats
    ImGui::Text("FPS: %.1f", 1.0f / timestep);
    ImGui::Text("Frame Time: %.3f ms", timestep * 1000.0f);
    
    ImGui::Separator();
    
    // Profiler stats
    ImGui::Text("Profiler Statistics");
    
    // Use our isDebugBuild() method instead of the preprocessor check
    if (Profiler::isDebugBuild()) {
        ImGui::Text("Last Frame Time: %.3f ms", Profiler::getLastFrameTime());
        ImGui::Text("Average Frame Time: %.3f ms", Profiler::getAverageFrameTime());
        ImGui::Text("Min Frame Time: %.3f ms", Profiler::getMinFrameTime());
        ImGui::Text("Max Frame Time: %.3f ms", Profiler::getMaxFrameTime());
        ImGui::Text("FPS: %d", Profiler::getFramesPerSecond());
        
        // Frame time history graph
        ImGui::Separator();
        ImGui::Text("Frame Time History");
        
        const auto& frameTimeHistory = Profiler::getFrameTimeHistory();
        
        // Find the maximum value for scaling
        float maxValue = 0.0f;
        for (const auto& frametime : frameTimeHistory) {
            maxValue = std::max(maxValue, frametime);
        }
        
        // Add a little headroom to the max value
        maxValue = maxValue * 1.2f;
        
        // Plot the frame times
        ImGui::PlotLines("##frametimes", 
                        frameTimeHistory.data(), 
                        frameTimeHistory.size(), 
                        0,                      // values offset
                        "ms",                  // overlay text
                        0.0f,                  // scale min
                        maxValue,              // scale max
                        ImVec2(0, 80));        // graph size
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 
                          "Detailed profiling is only available in debug builds");
        
        // Add instructions to force debug build
        ImGui::TextWrapped("To enable profiling, build the project in debug mode using 'build_debug.bat'");
        ImGui::TextWrapped("If you're already using this script, check that the build configuration in Visual Studio is set to 'Debug'");
    }
    
    ImGui::End();
}

}  // namespace Rapture 