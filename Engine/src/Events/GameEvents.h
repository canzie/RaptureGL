#pragma once
#include "Event.h"

// This header is kept for backward compatibility.
// It uses the unified event system from Event.h.

namespace Rapture {
    // For backward compatibility, define these aliases
    namespace GameEvents {
        // Scene events
        inline auto& onSceneLoadRequested = onSceneLoadRequested();
        inline auto& onSceneActivated = onSceneActivated();
        inline auto& onSceneDeactivated = onSceneDeactivated();
        
        // World events
        inline auto& onWorldTransitionRequested = onWorldTransitionRequested();
        inline auto& onWorldActivated = onWorldActivated();
        
        // Layer communication events
        inline auto& onLayerCommunication = onLayerCommunication();
        
        // Project events
        inline auto& onProjectLoadRequested = onProjectLoadRequested();
        inline auto& onProjectLoaded = onProjectLoaded();


        inline auto& onEntitySelected = onEntitySelected();
    }
} 