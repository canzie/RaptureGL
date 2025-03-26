# Implement deferred renderer

### What
Deferred rendering separates the geometry and lighting passes, making it efficient for scenes with many light sources. It renders scene properties (position, normal, albedo, etc.) to a G-buffer first, then applies lighting in a second pass.

## Optimal Way
1. Create a G-buffer with multiple render targets:
   - Position buffer (RGB32F or RGB16F if precision allows)
   - Normal buffer (RGB16F)
   - Albedo + Specular buffer (RGBA8)
   - Material properties buffer (roughness, metallic, etc.) (RGBA8/16F)
   - Depth buffer (DEPTH24STENCIL8)

2. Implement two-pass rendering pipeline:
   - Geometry pass: Fill G-buffer with material/geometry data
   - Lighting pass: Calculate lighting using G-buffer data
   - Optional: Add stencil optimization for light volumes

## Plan
1. Extend the Framebuffer class to support multiple render targets (MRT)
   - Implement format-specific attachment creation
   - Add validation for MRT compatibility
   - Create clear methods for each buffer type
   - Support for binding specific buffer subsets

2. Create G-buffer shaders for the geometry pass
   - Implement efficient data packing in shaders
   - Split material parameters based on update frequency
   - Use shader storage buffer objects (SSBOs) for material data

3. Implement lighting pass shader using screen-space quad
   - Create single full-screen quad VAO/VBO for all post-processing
   - Implement light batching for similar light types
   - Use compute shaders for tile-based deferred shading on high-density light scenes

4. Add support for different light types (point, directional, spot)
   - Implement light culling with spatial data structures
   - Use stencil buffer to optimize light volume rendering
   - Batch similar lights to minimize shader switches

5. Implement optional post-processing effects (bloom, ambient occlusion, etc.)
   - Create composable post-process pipeline
   - Support for render passes with dependencies
   - Implement downsampling for effects like bloom

## Optimizations
1. Memory bandwidth optimizations:
   - Pack G-buffer data efficiently (e.g., normal encoding, compress when possible)
   - Consider half-precision floats where appropriate
   - Use texture arrays for similar-format buffers

2. Draw call optimizations:
   - Implement material-based batching for geometry pass
   - Use instancing for similar meshes
   - Implement frustum and occlusion culling before geometry pass

3. Shader optimizations:
   - Implement shader permutations for different material types
   - Use shader hot-reloading system for development
   - Profile and optimize critical shader paths

## What needs to be changed/added
- Create `DeferredRenderer` class extending current renderer
  - Implement render queue with sorting by material/shader
  - Add visibility determination system
  - Create light management system with culling

- Add MRT support to Framebuffer implementation
  - Support for different attachment formats and counts
  - Add framebuffer validation and error reporting
  - Implement efficient framebuffer resizing

- Create shader library with geometry pass and lighting pass shaders
  - Add uniform buffer binding points for global data
  - Create material parameter system with auto-binding
  - Implement shader reflection for automatic UI generation

- Add support for multiple light types and shadow maps
  - Implement cascaded shadow maps for directional lights
  - Add shadow atlas for point/spot lights
  - Support for efficient soft shadows

- Implement screen-space quad rendering for post-processing
  - Create reusable post-process framework
  - Support for effect stacking and dependencies
  - Add gamma correction and HDR tone mapping


# Improved Buffer management (IBOs, VBOs and VAOs)

## Optimal way
1. Implement a buffer management system using OpenGL's DSA (Direct State Access) where possible
2. Add support for immutable storage buffers (glBufferStorage)
3. Implement a proper memory mapping strategy for dynamic buffers
4. Use buffer orphaning, persistent mapping, or buffer streaming for frequently updated data

## Plan
1. Redesign buffer classes to use modern OpenGL features
   - Implement wrapper classes for different buffer types (vertex, index, uniform, storage)
   - Add state tracking to minimize redundant state changes
   - Create buffer creation flags for different usage patterns
   - Support extension detection for fallbacks on older hardware

2. Implement different buffer usage patterns (static, dynamic, stream)
   - Create specialized buffer classes optimized for each pattern
   - Add buffer suballocation for small, frequently changed data
   - Use fences for synchronization with persistent mapping
   - Implement triple-buffering for frequently updated stream buffers

3. Support for buffer orphaning, persistent mapping for dynamic buffers
   - Implement ring buffer for dynamic data
   - Add buffer pooling for similarly sized allocations
   - Create memory barriers manager for compute/storage operations
   - Implement explicit synchronization for multi-threaded buffer updates

4. Add support for instanced rendering with instance buffers
   - Create dedicated instance buffer class
   - Implement automatic instance data packing
   - Add support for instance culling before buffer updates
   - Create instanced material property system

## Memory Management
1. Buffer pool implementation:
   - Group buffers by size/usage for efficient reuse
   - Add deferred deletion for buffers still in use by GPU
   
2. Multi-threaded considerations (DO NOT MAKE IT MUTLI-THREADED, JUST CONSIDERATIONS):
   - Create thread-safe buffer management
   - Implement command queue for buffer operations
   - Use sync objects for cross-thread buffer sharing

## What needs to be changed/added
- Refactor `VertexBuffer` and `IndexBuffer` classes to use DSA when available
  - Add version detection and fallback paths
  - Implement buffer usage hints based on update frequency
  - Create debug markers for graphics debuggers

- Implement batched buffer updates to reduce state changes
  - Create buffer update queue to batch similar operations
  - Add buffer usage statistics for performance monitoring
  - Implement automatic buffer usage pattern detection

- Add support for buffer objects with immutable storage
  - Create explicit synchronization for persistent mapping
  - Implement multi-buffering for frequently updated data
  - Add memory barrier management for compute/storage operations

- Create buffer pool/manager for reusing buffer objects
  - Implement reference counting for shared buffers
  - Add buffer resurrection to minimize reallocation
  - Create allocation strategy based on usage patterns

- Add support for Uniform Buffer Objects (UBO) and Shader Storage Buffer Objects (SSBO)
  - Implement UBO binding point management
  - Create automatic UBO layout packing
  - Add shader reflection for automatic UBO/SSBO binding
  - Implement std140/std430 layout compatibility helpers


# Mesh management

## How to handle complex meshes with a lot of submeshes
A robust mesh system should use a hierarchical structure with support for instancing and efficient rendering.

### How can we make sure each submesh can have its own set of components
1. Create a component-based model where each submesh can have its own material, transform, and other properties
2. Use a scene graph structure where submeshes inherit properties from parent meshes but can override them
3. Implement property inheritance with override flags

## Optimal Way
1. Implement a scene graph with nodes for transforms and meshes
   - Support efficient transform hierarchy updates
   - Add dirty flags for changed transforms
   - Implement spatial queries for visibility determination
   - Support for world/local space conversions

2. Support mesh instancing for rendering many identical objects efficiently
   - Create instance buffer management system
   - Implement automatic instancing detection
   - Add support for per-instance material property overrides
   - Create culling system that works with instanced rendering

3. Use material system with inheritance for shared properties
   - Implement material templates and instances
   - Create efficient material parameter storage
   - Add material sorting for minimizing state changes
   - Support for material variants (e.g., standard, wireframe, shadow)

## Geometry Management
1. Mesh data optimization:
   - Implement vertex cache optimization
   - Add support for vertex data compression
   - Support for mesh simplification algorithms

2. Draw call batching:
   - Group similar submeshes by material
   - Implement GPU mesh culling using compute shaders
   - Add support for indirect drawing commands
   - Create automatic mesh sorting for transparent objects

## Plan
1. Create a hierarchical mesh system with parent-child relationships
   - Implement efficient transform update propagation
   - Add support for local/world transform queries
   - Create attachment points for bones/sockets
   - Implement efficient transform interpolation for animations

2. Extend the Entity Component System to support submesh-specific components
   - Create component containers at submesh level
   - Implement component inheritance from parent meshes
   - Add override flags for component properties
   - Support for component event propagation in hierarchy

3. Implement efficient frustum culling for submeshes
   - Create hierarchical bounding volume system
   - Implement spatial partitioning for scene objects
   - Add support for occlusion culling

## What needs to be changed/added
- Implement a proper scene graph with transform hierarchies
  - Create efficient transform update system
  - Add spatial query support for visibility testing
  - Implement transform change notification system
  - Support for transform interpolation and animation

- Enhance `Mesh` and `SubMesh` classes to support hierarchical relationships
  - Add material override system for submeshes
  - Implement efficient submesh culling
  - Create submesh-specific property containers
  - Support for submesh visibility toggling

- Add instanced rendering for identical meshes with different transforms
  - Implement instance buffer management
  - Create automatic instancing for similar objects
  - Add support for per-instance property overrides
  - Implement efficient instance culling




- improved buffer management trough 16-64mb pre-allocated buffers
- instanced meshes
- particle system
- deffered renderer
- mouse picking
- Physics
- Audio
- serializer
- more advanced particle system
- Scripting






