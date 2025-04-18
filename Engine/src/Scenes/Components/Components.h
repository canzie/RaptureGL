//#include <glm/glm.hpp>

#pragma once

#include "../../Materials/Material.h"
#include "../../Materials/MaterialLibrary.h"
#include "../../Mesh/Mesh.h"
#include "../../Camera/PerspectiveCamera.h"
#include "../../Scenes/EntityNode.h" // Forward declare instead
#include "Transforms.h"
#include "BoundingBox.h"
#include "../../Debug/Profiler.h"
#include "../../Textures/Texture.h"

#include "../../Renderer/PrimitiveShapes.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../Animations/Skeleton/Skeleton.h"
#include "../../Animations/Animation.h"

#include "../../AssetsManager/AssetManager.h"

#include "../../Renderer/Frustum.h"
#include "../../Renderer/ShadowMapping/CascadedShadowMapping.h"
#include "../../Renderer/ShadowMapping/ShadowMapping.h"

#include <vector>
#include <memory> // For shared_ptr
//#include <string>

namespace Rapture {




	struct TransformComponent
	{
		Transforms transforms;
        
        glm::vec3 translation() const { return transforms.getTranslation(); }
        glm::vec3 rotation() const { return transforms.getRotation(); }
        glm::vec3 scale() const { return transforms.getScale(); }
        glm::mat4 transformMatrix() const { return transforms.getTransform(); }

        private:
            mutable std::size_t m_lastHash = 0;

        public:
        TransformComponent()
        {
            transforms = Transforms();
        }

        TransformComponent(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale) {
            transforms = Transforms(translation, rotation, scale);
        }

        // Add constructor for quaternion rotation
        TransformComponent(glm::vec3 translation, glm::quat rotation, glm::vec3 scale) {
            transforms = Transforms(translation, rotation, scale);
        }

        TransformComponent(glm::mat4 transformMatrix) {
            transforms.setTransform(transformMatrix);
        }

        std::size_t calculateCurrentHash() const
        {
            const glm::mat4& matrix = transforms.getTransform();
            std::size_t hash = 0;
            
            // Hash the 16 float values in the transform matrix
            const float* values = glm::value_ptr(matrix);
            for (int i = 0; i < 16; ++i) {
                // Simple hash combination using bit operations
                // Multiply by prime number and XOR with current hash
                hash ^= std::hash<float>{}(values[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            
            return hash;
        }

        bool hasChanged() const
        {
            uint32_t currentHash = calculateCurrentHash();
            if (m_lastHash != currentHash) {
                m_lastHash = currentHash;
                return true;
            }
            return false;
        }
	};

    struct AnimationComponent
    {
        std::shared_ptr<Animation> animation;
        std::string animationName;
        std::vector<std::shared_ptr<Animation>> animations;
        int currentAnimationIndex = 0;
        bool autoPlay = false;
        
        AnimationComponent() = default;
        
        AnimationComponent(std::shared_ptr<Animation> anim)
            : animation(anim), animationName(anim ? anim->getName() : "")
        {
            if (anim) {
                animations.push_back(anim);
            }
        }
        
        AnimationComponent(std::vector<std::shared_ptr<Animation>> anims)
            : animations(anims)
        {
            if (!animations.empty()) {
                animation = animations[0];
                animationName = animation->getName();
            }
        }
        
        void playAnimation()
        {
            if (animation) {
                animation->play();
            }
        }
        
        void pauseAnimation()
        {
            if (animation) {
                animation->pause();
            }
        }
        
        void stopAnimation()
        {
            if (animation) {
                animation->stop();
            }
        }
        
        void resetAnimation()
        {
            if (animation) {
                animation->reset();
            }
        }
        
        void setAnimation(int index)
        {
            if (index >= 0 && index < animations.size()) {
                animation = animations[index];
                animationName = animation->getName();
                currentAnimationIndex = index;
            }
        }
        
        void update(float deltaTime)
        {
            if (animation) {
                animation->update(deltaTime);
            }
        }
        
        void applyToSkeleton(std::shared_ptr<Skeleton> skeleton)
        {
            if (animation && skeleton) {
                animation->applyToSkeleton(skeleton);
            }
        }
    };

    struct SkeletonComponent
    {
        std::shared_ptr<Skeleton> skeleton;
        
        SkeletonComponent(std::string& name)
        {
            skeleton = std::make_shared<Skeleton>(name);
        }

        SkeletonComponent()
        {
            skeleton = std::make_shared<Skeleton>();
        }
        
        
    };


    struct StaticMeshComponent
    {
        std::shared_ptr<Mesh> mesh;
        bool isLoading = false;
        
        
    };

	struct MeshComponent
	{

    private:
        //AssetHandle m_assetHandle;
        std::weak_ptr<Mesh> m_weakMesh;

    public:
		std::shared_ptr<Mesh> mesh;
        
        bool isLoading = false;
		
		MeshComponent(std::string fname)
		{
			mesh = std::make_shared<Mesh>(fname);
            isLoading = true;
            GE_CORE_INFO("Loading mesh with glTF Loader: {0}", fname);
		}
        
        MeshComponent(std::string fname, bool useGLTF2)
        {
            // TODO: Implement glTF2 loading
            mesh = std::make_shared<Mesh>(fname);
            isLoading = true;
            GE_CORE_INFO("Loading mesh with glTF2 Loader: {0}", fname);
        }
        
        // Constructor that takes a mesh shared_ptr
        MeshComponent(std::shared_ptr<Mesh> meshPtr) : mesh(meshPtr), isLoading(false) {}
        
        MeshComponent(bool isEmpty)
		{
			if (isEmpty)
			{
				mesh = std::make_shared<Mesh>();
                isLoading = true;
			}
            else
            {
                MeshComponent();
            }
		}

		MeshComponent()
		{
			mesh = Mesh::createCube();
		}

	};
    
    // Note: BoundingBoxComponent is now defined in BoundingBox.h
    
	struct MaterialComponent
	{
    private:
        std::weak_ptr<Material> m_weakMaterial;
        AssetHandle m_assetHandle;

    public:


		std::shared_ptr<Material> material;
        std::string materialName;

        MaterialComponent(const AssetHandle& handle) : m_assetHandle(handle)
        {
            m_weakMaterial = AssetManager::getAsset<Material>(handle);
            materialName = m_weakMaterial.lock()->getName();
            material = m_weakMaterial.lock();
        }

		MaterialComponent()
		{
			// Create a default metal material with dark gray color
			auto [material, handle] = AssetManager::getDefaultAsset<Material>(AssetType::Material);
            if (material) {
                this->material = material;
                this->m_assetHandle = handle;
                materialName = material->getName();
            }
            else {
                MaterialComponent(glm::vec3(0.5f, 0.5f, 0.5f));
                GE_CORE_ERROR("MaterialComponent: Failed to get default material");
            }
		}

        MaterialComponent(glm::vec3 base_color)
        {
            material = MaterialLibrary::createSolidMaterial(
                "SolidMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this)),
                base_color
            );
            materialName = material->getName();
        }

		
		MaterialComponent(glm::vec3 base_color, float roughness, float metallic, float specular)
		{
			// Create a custom PBR material with the provided parameters
			material = MaterialLibrary::createPBRMaterial(
				"CustomMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this)), 
				base_color, 
				roughness, 
				metallic, 
				specular
			);
            materialName = material->getName();
		}
		
		MaterialComponent(const std::string& materialName)
		{
			// Use an existing material from the library
			material = MaterialLibrary::getMaterial(materialName);
            this->materialName = materialName;
		}

        MaterialComponent(std::shared_ptr<Material> material)
        {
            this->material = material;
            this->materialName = material->getName();
        }

        void setMaterial(const AssetHandle& handle)
        {
            m_weakMaterial = AssetManager::getAsset<Material>(handle);
            m_assetHandle = handle;
            materialName = m_weakMaterial.lock()->getName();
            material = m_weakMaterial.lock();
        }
		
		~MaterialComponent()
		{
			// No need to manually delete the material as it's now managed by shared_ptr
		}
		
		// Methods to modify material properties after creation
		
		// Set base color (works for both PBR and Solid materials)
		void setBaseColor(const glm::vec4& color)
		{
			if (material)
			{
				// For PBR materials, the parameter name is typically "baseColor"
				if (material->getType() == MaterialType::PBR)
				{
					material->setVec4("baseColor", color);
				}
				// For solid materials, it might be called "color"
				else if (material->getType() == MaterialType::SOLID)
				{
					material->setVec3(ParameterID::BASE_COLOR, glm::vec3(color));
				}
			}
		}
		
		// Set roughness (PBR materials only)
		void setRoughness(float roughness)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setFloat("roughness", roughness);
			}
		}
		
		// Set metallic value (PBR materials only)
		void setMetallic(float metallic)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setFloat("metallic", metallic);
			}
		}
		
		// Set specular value (PBR materials only)
		void setSpecular(float specular)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setFloat("specular", specular);
			}
		}
		
		// Additional helper to change all PBR properties at once
		void setPBRProperties(const glm::vec3& baseColor, float roughness, float metallic, float specular)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setVec4("baseColor", glm::vec4(baseColor, 1.0f));
				material->setFloat("roughness", roughness);
				material->setFloat("metallic", metallic);
				material->setFloat("specular", specular);
			}
		}
		
		// Get current material properties
		glm::vec3 getBaseColor() const
		{
			if (material)
			{
				if (material->getType() == MaterialType::PBR && material->hasParameter("baseColor"))
				{
					return glm::vec3(material->getParameter("baseColor").asVec4());
				}
				else if (material->getType() == MaterialType::SOLID && material->hasParameter(ParameterID::BASE_COLOR))
				{
					return glm::vec3(material->getParameter(ParameterID::BASE_COLOR).asVec3());
				}
			}
			return glm::vec3(0.0f);
		}
		
		float getRoughness() const
		{
			if (material && material->getType() == MaterialType::PBR && material->hasParameter("roughness"))
			{
				return material->getParameter("roughness").asFloat();
			}
			return 0.0f;
		}
		
		float getMetallic() const
		{
			if (material && material->getType() == MaterialType::PBR && material->hasParameter("metallic"))
			{
				return material->getParameter("metallic").asFloat();
			}
			return 0.0f;
		}
		
		float getSpecular() const
		{
			if (material && material->getType() == MaterialType::PBR && material->hasParameter("specular"))
			{
				return material->getParameter("specular").asFloat();
			}
			return 0.0f;
		}
	};


    // could split this into a CameraComponent and a CameraControllerComponent
    // but I cba
    // frustum would then be added to the CameraComponent, since a camera
	struct CameraControllerComponent
	{
		PerspectiveCamera camera;

        Frustum frustum;

		float fov;
		float aspect_ratio;
		float near_plane;
		float far_plane;

		glm::vec3 translation;
		glm::vec3 cameraFront;

		float yaw;
		float pitch;

		glm::vec3 rotation_axis;
		float rotation_angle;

		CameraControllerComponent(float fovy, float AR, float nplane, float fplane)
		{
			fov = fovy;
			aspect_ratio = AR;
			near_plane = nplane;
			far_plane = fplane;
			camera = PerspectiveCamera(fovy, AR, nplane, fplane);


			rotation_angle = 0.0f;
			rotation_axis = { 1.0f, 0.0f, 0.0f };
			translation = { 0.0f, 0.0f, -3.0f };
			cameraFront = { 0.0f, 0.0f, 1.0f };

			yaw = -90.0f;
			pitch = 0.0f;

			camera.updateViewMatrix(translation);


		}

        void updateProjectionMatrix(float fovy, float AR, float nplane, float fplane)
        {
            fov = fovy;
            aspect_ratio = AR;
            near_plane = nplane;
            far_plane = fplane;
            camera.updateProjectionMatrix(fovy, AR, nplane, fplane);
        }

	};

    struct SpriteComponent
    {
        std::shared_ptr<Texture2D> texture;
        std::string texturePath;
        Quad quad;

        SpriteComponent() = default;

        SpriteComponent(std::string texturePath)
        {
            this->texturePath = texturePath;
            auto [tex, handle] = AssetManager::importAsset<Texture2D>(std::filesystem::path(texturePath));
            texture = tex;
            //texture = TextureLibrary::loadAsync(texturePath);
            quad = Quad();
            quad.getMaterial()->setTexture(ParameterID::TEXTURE_ALBEDO, texture, handle);
        };

        void setTexture(std::string texturePath)
        {
            this->texturePath = texturePath;
            auto [tex, handle] = AssetManager::importAsset<Texture2D>(std::filesystem::path(texturePath));
            texture = tex;
            //texture = TextureLibrary::loadAsync(texturePath);
            quad.getMaterial()->setTexture(ParameterID::TEXTURE_ALBEDO, texture, handle);
        }
        

    };


    struct GizmoComponent
    {
        bool isActive = false;
        
        

    };

    // is not enforced at all right now
    // only way i can think of would be to copy-paste the entityNodeComponent
    // but having both is weird and only having one can lead to inconsistencies
    // so this is just a dummy component to get a root entity quickly, witouth having to traverse the entire hierarchy
    struct RootComponent
    {
        bool yo = true;

        RootComponent() = default;
        

    };

    struct SkeletonRefComponent
    {
        const std::weak_ptr<Skeleton> skeleton;

        SkeletonRefComponent(std::shared_ptr<Skeleton> skeleton) : skeleton(skeleton) {}
        
    };

    struct EntityNodeComponent
    {
        std::shared_ptr<EntityNode> entity_node;

        EntityNodeComponent() = default;

        EntityNodeComponent(std::shared_ptr<Entity> entity)
        {
            entity_node = std::make_shared<EntityNode>(entity);
        }

        EntityNodeComponent(std::shared_ptr<Entity> entity, std::shared_ptr<EntityNode> parent)
        {
            entity_node = std::make_shared<EntityNode>(entity, parent);
        }
        
        // Add constructors that accept Entity directly
        EntityNodeComponent(Entity entity)
        {
            entity_node = std::make_shared<EntityNode>(std::make_shared<Entity>(entity));
        }

        EntityNodeComponent(Entity entity, std::shared_ptr<EntityNode> parent)
        {
            entity_node = std::make_shared<EntityNode>(std::make_shared<Entity>(entity), parent);
        }

        ~EntityNodeComponent()
        {
        }
        
        
    };

    struct ComputeTextureComponent
    {
        std::shared_ptr<Shader> shader = nullptr;
        std::shared_ptr<Texture2D> texture = nullptr;

        const unsigned int localSizeX = 32;
        const unsigned int localSizeY = 32;
        unsigned int numGroupsX = 0;
        unsigned int numGroupsY = 0;

        ComputeTextureComponent() = default;

        ComputeTextureComponent(std::shared_ptr<Shader> shader, std::shared_ptr<Texture2D> texture)
        {
            this->shader = shader;
            this->texture = texture;

            numGroupsX = (texture->getWidth() + localSizeX - 1) / localSizeX;  // Ceiling division
            numGroupsY = (texture->getHeight() + localSizeY - 1) / localSizeY; // Ceiling division
        }

        void compute() {
            if (shader && texture) {

                shader->bind();
                texture->bindCompute(0);
                // Dispatch the calculated number of work groups
                shader->dispatchCompute(numGroupsX, numGroupsY, 1);
                shader->unBind();
            }
        }
        
    };

    struct TagComponent
    {
        std::string tag;

        TagComponent(const std::string& tag) : tag(tag) {}
        
    };

    // Light types for the LightComponent
    enum class LightType
    {
        Point = 0,
        Directional = 1,
        Spot = 2
    };


    // find a way to force some components to have a certain other component in their entity
    // i.e. a shadow must have a light.
    struct ShadowComponent
    {
        bool isActive = false;
        std::shared_ptr<ShadowMap> shadowMap = nullptr;
        
        // Cache shadow map size
        uint32_t width = 2048;
        uint32_t height = 2048;
        
        // Shadow map size for directional lights (controls the orthographic projection size)
        float shadowMapSize = 100.0f;

        std::shared_ptr<Frustum> frustum = nullptr;

        ShadowComponent(uint32_t width=2048, uint32_t height=2048) 
            : width(width), height(height) {
                isActive = true;
                shadowMap = std::make_shared<ShadowMap>(width, height);
                frustum = std::make_shared<Frustum>();
        }

        void updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
            frustum->update(projectionMatrix, viewMatrix);
        }
    };

    struct CascadedShadowComponent
    {
        bool isActive = false;
        uint8_t numCascades = 4;
        std::shared_ptr<CascadedShadowMapping> cascadedShadowMapping = nullptr;  // Using ShadowMap for now, will be replaced with CSM later

        std::shared_ptr<Frustum> frustum = nullptr;

        // Cache shadow map size
        uint32_t width = 1024;
        uint32_t height = 1024;

        CascadedShadowComponent(uint32_t width=1024, uint32_t height=1024, uint8_t numCascades=4) 
            : width(width), height(height), numCascades(numCascades) {
                cascadedShadowMapping = std::make_shared<CascadedShadowMapping>(width, height, numCascades);
                frustum = std::make_shared<Frustum>();
                isActive = true;
            }

        void updateFrustum() {
            if (cascadedShadowMapping) {
                frustum->update(cascadedShadowMapping->getOverallProjectionMatrix(), cascadedShadowMapping->getOverallViewMatrix());
            }
        }

    };

    struct LightComponent
    {

        LightType type = LightType::Point;
        glm::vec3 color = glm::vec3(1.0f, 0.8f, 0.6f);    // Light color (default: warm white?) #FFDDAA
        float intensity = 1.0f;               // Light intensity multiplier
        
        // For point and spot lights
        float range = 10.0f;                  // Attenuation range
        
        // For spot lights only
        float innerConeAngle = glm::radians(30.0f); // Inner cone angle in radians
        float outerConeAngle = glm::radians(45.0f); // Outer cone angle in radians
        
        // Flag indicating if the light is active
        bool isActive = true;
        bool castsShadow = false;
        
        private:
            mutable uint32_t m_lastHash = 0;

        public:
        // Constructors
        LightComponent() = default;
        
        // Constructor for point light
        LightComponent(const glm::vec3& color, float intensity, float range)
            : type(LightType::Point), color(color), intensity(intensity), range(range) {}
        
        // Constructor for directional light
        LightComponent(const glm::vec3& color, float intensity)
            : type(LightType::Directional), color(color), intensity(intensity) {}
        
        // Constructor for spot light
        LightComponent(const glm::vec3& color, float intensity, float range, 
                      float innerAngleDegrees, float outerAngleDegrees)
            : type(LightType::Spot), color(color), intensity(intensity), range(range),
              innerConeAngle(glm::radians(innerAngleDegrees)), 
              outerConeAngle(glm::radians(outerAngleDegrees)) {}



        std::uint32_t calculateCurrentHash() const {
            std::uint32_t hash = 0;
            
            // Common properties for all light types
            hash ^= std::hash<LightType>{}(type) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<bool>{}(isActive) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<bool>{}(castsShadow) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<float>{}(intensity) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            
            // Hash color components
            hash ^= std::hash<float>{}(color.r) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<float>{}(color.g) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<float>{}(color.b) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            
            // Properties specific to light types
            switch (type) {
                case LightType::Point:
                    // Point lights use range
                    hash ^= std::hash<float>{}(range) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    break;
                    
                case LightType::Directional:
                    // Directional lights don't need additional properties
                    break;
                    
                case LightType::Spot:
                    // Spot lights use range and cone angles
                    hash ^= std::hash<float>{}(range) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= std::hash<float>{}(innerConeAngle) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= std::hash<float>{}(outerConeAngle) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    break;
            }
            
            return hash;
        }

        bool hasChanged() const {

            uint32_t currentHash = calculateCurrentHash();
            if (m_lastHash != currentHash) {
                m_lastHash = currentHash;
                return true;
            }
            return false;
        }

    };



}