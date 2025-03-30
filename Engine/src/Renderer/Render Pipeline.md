as of 30.03.2025

# Render Pipeline


// renderer should be able to draw an entire scene just by the scene
Renderer::SubmitScene(Scene s)

the scene will have some config variables like
- SceneSettings
	- Frustum Culling flag
- Skybox Information

The scene will also have a registry of entities

These entities will each posses a set of components
The components will contain data, which might be usefull for rendering, not all components will contain data which is usefull to the renderer


### Render Pipeline Overview

- Renderer receives scene
- Scene gets processes
- Commands will be made from the scene processing
- Commands will be sumbitted to a Queue
- The Queue(s) will be processed (e.g. sorting)
- The Queue(s) will be submitted to the renderer
- The renderer will execute the commands

this design should allow for modular system to be implemented
Different renderPasses can be represented as different queues being submitted to the renderer


### Command Types

- RenderGeometryCommand (can be used in a deferred rendering pipeline)
- RenderMeshCommand (for forward rendering)
- PostProcessCommand
- ...

Issues, how 




