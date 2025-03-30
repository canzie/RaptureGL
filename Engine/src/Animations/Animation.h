#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Animations/Skeleton/Skeleton.h"
#include "../Logger/Log.h"

namespace Rapture {

// Forward declarations
class AnimationSampler;
class AnimationChannel;

enum class InterpolationType {
    LINEAR,
    STEP,
    CUBICSPLINE
};

struct Keyframe {
    float timeStamp;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
};

class Animation {
public:
    Animation(const std::string& name, float duration);
    ~Animation();

    const std::string& getName() const { return _name; }
    float getDuration() const { return _duration; }
    
    void addChannel(std::shared_ptr<AnimationChannel> channel);
    std::shared_ptr<AnimationChannel> getChannel(const std::string& targetName) const;
    
    void update(float deltaTime);
    void applyToSkeleton(std::shared_ptr<Skeleton> skeleton);
    
    void play();
    void pause();
    void stop();
    void reset();
    
    void setLooping(bool looping) { _isLooping = looping; }
    bool isLooping() const { return _isLooping; }
    
    void setPlaybackSpeed(float speed) { _playbackSpeed = speed; }
    float getPlaybackSpeed() const { return _playbackSpeed; }
    
    bool isPlaying() const { return _isPlaying; }
    float getCurrentTime() const { return _currentTime; }
    
private:
    std::string _name;
    float _duration;
    float _currentTime;
    float _playbackSpeed;
    bool _isPlaying;
    bool _isLooping;
    
    std::vector<std::shared_ptr<AnimationChannel>> _channels;
    std::unordered_map<std::string, std::shared_ptr<AnimationChannel>> _channelMap;
};

class AnimationSampler {
public:
    AnimationSampler(InterpolationType interpolation);
    ~AnimationSampler();
    
    void addPositionKeyframe(float time, const glm::vec3& position);
    void addRotationKeyframe(float time, const glm::quat& rotation);
    void addScaleKeyframe(float time, const glm::vec3& scale);
    
    glm::vec3 evaluatePosition(float time) const;
    glm::quat evaluateRotation(float time) const;
    glm::vec3 evaluateScale(float time) const;
    
    void sortKeyframes();
    
private:
    template<typename T>
    T interpolateLinear(const T& a, const T& b, float t) const;
    
    glm::quat interpolateRotation(const glm::quat& a, const glm::quat& b, float t) const;
    
    template<typename T>
    int findKeyframeIndex(const std::vector<std::pair<float, T>>& keyframes, float time) const;
    
    InterpolationType _interpolationType;
    std::vector<std::pair<float, glm::vec3>> _positionKeyframes;
    std::vector<std::pair<float, glm::quat>> _rotationKeyframes;
    std::vector<std::pair<float, glm::vec3>> _scaleKeyframes;
};

class AnimationChannel {
public:
    AnimationChannel(const std::string& targetBone);
    ~AnimationChannel();
    
    const std::string& getTargetBone() const { return _targetBone; }
    
    void setPositionSampler(std::shared_ptr<AnimationSampler> sampler) { _positionSampler = sampler; }
    void setRotationSampler(std::shared_ptr<AnimationSampler> sampler) { _rotationSampler = sampler; }
    void setScaleSampler(std::shared_ptr<AnimationSampler> sampler) { _scaleSampler = sampler; }
    
    glm::vec3 evaluatePosition(float time) const;
    glm::quat evaluateRotation(float time) const;
    glm::vec3 evaluateScale(float time) const;
    
    bool hasPositionSampler() const { return _positionSampler != nullptr; }
    bool hasRotationSampler() const { return _rotationSampler != nullptr; }
    bool hasScaleSampler() const { return _scaleSampler != nullptr; }
    
private:
    std::string _targetBone;
    std::shared_ptr<AnimationSampler> _positionSampler;
    std::shared_ptr<AnimationSampler> _rotationSampler;
    std::shared_ptr<AnimationSampler> _scaleSampler;
};

class AnimationManager {
public:
    static AnimationManager& getInstance() {
        static AnimationManager instance;
        return instance;
    }
    
    void addAnimation(std::shared_ptr<Animation> animation);
    std::shared_ptr<Animation> getAnimation(const std::string& name);
    void removeAnimation(const std::string& name);
    
    void update(float deltaTime);
    
private:
    AnimationManager() = default;
    ~AnimationManager() = default;
    
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<Animation>> _animations;
    std::vector<std::shared_ptr<Animation>> _activeAnimations;
};

class AnimationBlender {
public:
    AnimationBlender();
    ~AnimationBlender();
    
    void addAnimation(std::shared_ptr<Animation> animation, float weight);
    void removeAnimation(const std::string& animationName);
    void setAnimationWeight(const std::string& animationName, float weight);
    float getAnimationWeight(const std::string& animationName) const;
    
    void normalizeWeights();
    void apply(std::shared_ptr<Skeleton> skeleton);
    
private:
    struct BlendedAnimation {
        std::shared_ptr<Animation> animation;
        float weight;
    };
    
    std::vector<BlendedAnimation> _blendedAnimations;
    std::unordered_map<std::string, size_t> _animationIndices;
};

}

