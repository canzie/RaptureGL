#include "Animation.h"

#include <algorithm>
#include <cassert>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "../Debug/TracyProfiler.h"

namespace Rapture {

// Animation class implementation
Animation::Animation(const std::string& name, float duration)
    : _name(name)
    , _duration(duration)
    , _currentTime(0.0f)
    , _playbackSpeed(1.0f)
    , _isPlaying(false)
    , _isLooping(true)
{
    GE_CORE_TRACE("Animation '{0}' created with duration {1}s", name, duration);
}

Animation::~Animation() {
    GE_CORE_TRACE("Animation '{0}' destroyed", _name);
}

void Animation::addChannel(std::shared_ptr<AnimationChannel> channel) {
    if (!channel) {
        GE_CORE_ERROR("Attempted to add null animation channel");
        return;
    }
    
    _channels.push_back(channel);
    _channelMap[channel->getTargetBone()] = channel;
    GE_CORE_TRACE("Added channel for bone '{0}' to animation '{1}'", channel->getTargetBone(), _name);
}

std::shared_ptr<AnimationChannel> Animation::getChannel(const std::string& targetName) const {
    auto it = _channelMap.find(targetName);
    if (it != _channelMap.end()) {
        return it->second;
    }
    return nullptr;
}

void Animation::update(float deltaTime) {
    if (!_isPlaying) {
        return;
    }
    
    _currentTime += deltaTime * _playbackSpeed;
    
    // Handle looping
    if (_currentTime > _duration) {
        if (_isLooping) {
            _currentTime = fmod(_currentTime, _duration);
        } else {
            _currentTime = _duration;
            _isPlaying = false;
        }
    } else if (_currentTime < 0.0f) {
        if (_isLooping) {
            _currentTime = _duration + fmod(_currentTime, _duration);
        } else {
            _currentTime = 0.0f;
            _isPlaying = false;
        }
    }
}

void Animation::applyToSkeleton(std::shared_ptr<Skeleton> skeleton) {
    // For each channel in the animation
    RAPTURE_PROFILE_FUNCTION();
    std::unordered_map<std::string, glm::mat4> boneTransforms;
    
    for (const auto& channel : _channels) {
        auto bone = skeleton->getBone(channel->getTargetBone());

        if (bone) {
            // Evaluate all transforms at the current time
            glm::vec3 position = channel->evaluatePosition(_currentTime);
            glm::quat rotation = channel->evaluateRotation(_currentTime);
            glm::vec3 scale = channel->evaluateScale(_currentTime);
            
            // Create transform matrix from components
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, position);
            transform = transform * glm::mat4_cast(rotation);
            transform = glm::scale(transform, scale);

            if (boneTransforms.find(bone->name) == boneTransforms.end()) {
                boneTransforms[bone->name] = transform;
                bone->transform = boneTransforms[bone->name];

            } else {
                boneTransforms[bone->name] = boneTransforms[bone->name] * transform;
                bone->transform = boneTransforms[bone->name];

            }

            // Update bone transform
        } else {
            //GE_CORE_WARN("Bone '{0}' not found in skeleton", channel->getTargetBone());
        }
    }
    
    
    // Propagate changes through skeleton
    const auto& rootBone = skeleton->getRootBone();
    auto bones = skeleton->getBones();
    if (rootBone && !bones.empty()) {
        // Pass identity matrix as the starting parent world transform for the root bone
        skeleton->propegateBoneUpdate(bones[0], rootBone->transform);
    } else if (!bones.empty()) {
        skeleton->propegateBoneUpdate(bones[0], glm::mat4(1.0f));
    }
}

void Animation::play() {
    _isPlaying = true;
    GE_CORE_TRACE("Animation '{0}' started playing", _name);
}

void Animation::pause() {
    _isPlaying = false;
    GE_CORE_TRACE("Animation '{0}' paused at {1}s", _name, _currentTime);
}

void Animation::stop() {
    _isPlaying = false;
    _currentTime = 0.0f;
    GE_CORE_TRACE("Animation '{0}' stopped", _name);
}

void Animation::reset() {
    _currentTime = 0.0f;
    GE_CORE_TRACE("Animation '{0}' reset to beginning", _name);
}

// AnimationSampler implementation
AnimationSampler::AnimationSampler(InterpolationType interpolation)
    : _interpolationType(interpolation)
{
}

AnimationSampler::~AnimationSampler() {
}

void AnimationSampler::addPositionKeyframe(float time, const glm::vec3& position) {
    _positionKeyframes.push_back(std::make_pair(time, position));
}

void AnimationSampler::addRotationKeyframe(float time, const glm::quat& rotation) {
    _rotationKeyframes.push_back(std::make_pair(time, rotation));
}

void AnimationSampler::addScaleKeyframe(float time, const glm::vec3& scale) {
    _scaleKeyframes.push_back(std::make_pair(time, scale));
}

glm::vec3 AnimationSampler::evaluatePosition(float time) const {
    if (_positionKeyframes.empty()) {
        return glm::vec3(0.0f);
    }
    
    if (_positionKeyframes.size() == 1) {
        return _positionKeyframes[0].second;
    }
    
    int index = findKeyframeIndex(_positionKeyframes, time);
    
    // Handle boundary cases
    if (index == -1) {
        return _positionKeyframes.front().second;
    }
    
    if (index == static_cast<int>(_positionKeyframes.size()) - 1) {
        return _positionKeyframes.back().second;
    }
    
    // Get the two keyframes to interpolate between
    const auto& keyframe1 = _positionKeyframes[index];
    const auto& keyframe2 = _positionKeyframes[index + 1];
    
    // Calculate the interpolation factor
    float t1 = keyframe1.first;
    float t2 = keyframe2.first;
    float factor = (time - t1) / (t2 - t1);
    
    // Interpolate based on the interpolation type
    switch (_interpolationType) {
        case InterpolationType::STEP:
            return keyframe1.second;
            
        case InterpolationType::LINEAR:
            return interpolateLinear(keyframe1.second, keyframe2.second, factor);
            
        case InterpolationType::CUBICSPLINE:
            // Cubicspline interpolation not implemented yet
            GE_CORE_WARN("CubicSpline interpolation not implemented, falling back to linear");
            return interpolateLinear(keyframe1.second, keyframe2.second, factor);
            
        default:
            return interpolateLinear(keyframe1.second, keyframe2.second, factor);
    }
}

glm::quat AnimationSampler::evaluateRotation(float time) const {
    if (_rotationKeyframes.empty()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    
    if (_rotationKeyframes.size() == 1) {
        return _rotationKeyframes[0].second;
    }
    
    int index = findKeyframeIndex(_rotationKeyframes, time);
    
    // Handle boundary cases
    if (index == -1) {
        return _rotationKeyframes.front().second;
    }
    
    if (index == static_cast<int>(_rotationKeyframes.size()) - 1) {
        return _rotationKeyframes.back().second;
    }
    
    // Get the two keyframes to interpolate between
    const auto& keyframe1 = _rotationKeyframes[index];
    const auto& keyframe2 = _rotationKeyframes[index + 1];
    
    // Calculate the interpolation factor
    float t1 = keyframe1.first;
    float t2 = keyframe2.first;
    float factor = (time - t1) / (t2 - t1);
    
    // Interpolate based on the interpolation type
    switch (_interpolationType) {
        case InterpolationType::STEP:
            return keyframe1.second;
            
        case InterpolationType::LINEAR:
            return interpolateRotation(keyframe1.second, keyframe2.second, factor);
            
        case InterpolationType::CUBICSPLINE:
            // Cubicspline interpolation not implemented yet
            GE_CORE_WARN("CubicSpline interpolation not implemented, falling back to linear");
            return interpolateRotation(keyframe1.second, keyframe2.second, factor);
            
        default:
            return interpolateRotation(keyframe1.second, keyframe2.second, factor);
    }
}

glm::vec3 AnimationSampler::evaluateScale(float time) const {
    if (_scaleKeyframes.empty()) {
        return glm::vec3(1.0f);
    }
    
    if (_scaleKeyframes.size() == 1) {
        return _scaleKeyframes[0].second;
    }
    
    int index = findKeyframeIndex(_scaleKeyframes, time);
    
    // Handle boundary cases
    if (index == -1) {
        return _scaleKeyframes.front().second;
    }
    
    if (index == static_cast<int>(_scaleKeyframes.size()) - 1) {
        return _scaleKeyframes.back().second;
    }
    
    // Get the two keyframes to interpolate between
    const auto& keyframe1 = _scaleKeyframes[index];
    const auto& keyframe2 = _scaleKeyframes[index + 1];
    
    // Calculate the interpolation factor
    float t1 = keyframe1.first;
    float t2 = keyframe2.first;
    float factor = (time - t1) / (t2 - t1);
    
    // Interpolate based on the interpolation type
    switch (_interpolationType) {
        case InterpolationType::STEP:
            return keyframe1.second;
            
        case InterpolationType::LINEAR:
            return interpolateLinear(keyframe1.second, keyframe2.second, factor);
            
        case InterpolationType::CUBICSPLINE:
            // Cubicspline interpolation not implemented yet
            GE_CORE_WARN("CubicSpline interpolation not implemented, falling back to linear");
            return interpolateLinear(keyframe1.second, keyframe2.second, factor);
            
        default:
            return interpolateLinear(keyframe1.second, keyframe2.second, factor);
    }
}

void AnimationSampler::sortKeyframes() {
    std::sort(_positionKeyframes.begin(), _positionKeyframes.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
        
    std::sort(_rotationKeyframes.begin(), _rotationKeyframes.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
        
    std::sort(_scaleKeyframes.begin(), _scaleKeyframes.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
}

template<typename T>
T AnimationSampler::interpolateLinear(const T& a, const T& b, float t) const {
    return a * (1.0f - t) + b * t;
}

glm::quat AnimationSampler::interpolateRotation(const glm::quat& a, const glm::quat& b, float t) const {
    // Use spherical linear interpolation for rotations
    return glm::slerp(a, b, t);
}

template<typename T>
int AnimationSampler::findKeyframeIndex(const std::vector<std::pair<float, T>>& keyframes, float time) const {
    // Find the last keyframe with timestamp <= time
    // Binary search implementation
    int left = 0;
    int right = static_cast<int>(keyframes.size()) - 1;
    
    // Empty keyframes or time before first keyframe
    if (right < 0 || time < keyframes[0].first) {
        return -1;
    }
    
    // Time after or at last keyframe
    if (time >= keyframes[right].first) {
        return right;
    }
    
    // Binary search
    while (left <= right) {
        int mid = left + (right - left) / 2;
        
        if (keyframes[mid].first <= time && 
            (mid == static_cast<int>(keyframes.size()) - 1 || keyframes[mid + 1].first > time)) {
            return mid;
        } else if (keyframes[mid].first > time) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return left;
}

// AnimationChannel implementation
AnimationChannel::AnimationChannel(const std::string& targetBone)
    : _targetBone(targetBone)
    , _positionSampler(nullptr)
    , _rotationSampler(nullptr)
    , _scaleSampler(nullptr)
{
}

AnimationChannel::~AnimationChannel() {
}

glm::vec3 AnimationChannel::evaluatePosition(float time) const {
    if (_positionSampler) {
        return _positionSampler->evaluatePosition(time);
    }
    return glm::vec3(0.0f);
}

glm::quat AnimationChannel::evaluateRotation(float time) const {
    if (_rotationSampler) {
        return _rotationSampler->evaluateRotation(time);
    }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
}

glm::vec3 AnimationChannel::evaluateScale(float time) const {
    if (_scaleSampler) {
        return _scaleSampler->evaluateScale(time);
    }
    return glm::vec3(1.0f);
}

// AnimationManager implementation
void AnimationManager::addAnimation(std::shared_ptr<Animation> animation) {
    if (!animation) {
        GE_CORE_ERROR("Attempted to add null animation to manager");
        return;
    }
    
    _animations[animation->getName()] = animation;
    GE_CORE_TRACE("Added animation '{0}' to manager", animation->getName());
}

std::shared_ptr<Animation> AnimationManager::getAnimation(const std::string& name) {
    auto it = _animations.find(name);
    if (it != _animations.end()) {
        return it->second;
    }
    
    GE_CORE_WARN("Animation '{0}' not found in manager", name);
    return nullptr;
}

void AnimationManager::removeAnimation(const std::string& name) {
    auto it = _animations.find(name);
    if (it != _animations.end()) {
        GE_CORE_TRACE("Removed animation '{0}' from manager", name);
        _animations.erase(it);
    }
}

void AnimationManager::update(float deltaTime) {
    // Clear active animations list
    _activeAnimations.clear();
    
    // Find all playing animations
    for (const auto& [name, animation] : _animations) {
        if (animation->isPlaying()) {
            _activeAnimations.push_back(animation);
        }
    }
    
    // Update each active animation
    for (auto& animation : _activeAnimations) {
        animation->update(deltaTime);
    }
}

// AnimationBlender implementation
AnimationBlender::AnimationBlender() {
}

AnimationBlender::~AnimationBlender() {
}

void AnimationBlender::addAnimation(std::shared_ptr<Animation> animation, float weight) {
    if (!animation) {
        GE_CORE_ERROR("Attempted to add null animation to blender");
        return;
    }
    
    const std::string& name = animation->getName();
    auto it = _animationIndices.find(name);
    
    if (it != _animationIndices.end()) {
        // Update existing animation weight
        _blendedAnimations[it->second].weight = weight;
    } else {
        // Add new animation
        _animationIndices[name] = _blendedAnimations.size();
        _blendedAnimations.push_back({animation, weight});
    }
    
    GE_CORE_TRACE("Added animation '{0}' to blender with weight {1}", name, weight);
}

void AnimationBlender::removeAnimation(const std::string& animationName) {
    auto it = _animationIndices.find(animationName);
    if (it != _animationIndices.end()) {
        size_t index = it->second;
        size_t lastIndex = _blendedAnimations.size() - 1;
        
        // If not the last element, move the last element to this position
        if (index != lastIndex) {
            _blendedAnimations[index] = _blendedAnimations[lastIndex];
            _animationIndices[_blendedAnimations[index].animation->getName()] = index;
        }
        
        // Remove the last element and update map
        _blendedAnimations.pop_back();
        _animationIndices.erase(it);
        
        GE_CORE_TRACE("Removed animation '{0}' from blender", animationName);
    }
}

void AnimationBlender::setAnimationWeight(const std::string& animationName, float weight) {
    auto it = _animationIndices.find(animationName);
    if (it != _animationIndices.end()) {
        _blendedAnimations[it->second].weight = weight;
        GE_CORE_TRACE("Set animation '{0}' weight to {1}", animationName, weight);
    }
}

float AnimationBlender::getAnimationWeight(const std::string& animationName) const {
    auto it = _animationIndices.find(animationName);
    if (it != _animationIndices.end()) {
        return _blendedAnimations[it->second].weight;
    }
    return 0.0f;
}

void AnimationBlender::normalizeWeights() {
    float totalWeight = 0.0f;
    
    // Calculate total weight
    for (const auto& anim : _blendedAnimations) {
        totalWeight += anim.weight;
    }
    
    // Normalize weights
    if (totalWeight > 0.0f) {
        for (auto& anim : _blendedAnimations) {
            anim.weight /= totalWeight;
        }
    }
}

void AnimationBlender::apply(std::shared_ptr<Skeleton> skeleton) {
    if (!skeleton || _blendedAnimations.empty()) {
        return;
    }
    
    // If only one animation with significant weight, just apply it directly
    if (_blendedAnimations.size() == 1 || _blendedAnimations[0].weight > 0.99f) {
        _blendedAnimations[0].animation->applyToSkeleton(skeleton);
        return;
    }
    
    // Get all bones
    const auto& bones = skeleton->getBones();
    
    // Create temporary storage for blended transformations
    std::unordered_map<std::string, glm::vec3> blendedPositions;
    std::unordered_map<std::string, glm::quat> blendedRotations;
    std::unordered_map<std::string, glm::vec3> blendedScales;
    std::unordered_map<std::string, float> totalWeights;
    
    // Normalize weights if needed
    normalizeWeights();
    
    // For each animation, evaluate and blend transformations
    for (const auto& blendedAnim : _blendedAnimations) {
        float weight = blendedAnim.weight;
        if (weight <= 0.0f) continue;
        
        std::shared_ptr<Animation> animation = blendedAnim.animation;
        float time = animation->getCurrentTime();
        
        // Process each bone affected by this animation
        for (const auto& bone : bones) {
            const std::string& boneName = bone->name;
            std::shared_ptr<AnimationChannel> channel = animation->getChannel(boneName);
            
            if (channel) {
                // Accumulate transformations with weights
                if (channel->hasPositionSampler()) {
                    glm::vec3 position = channel->evaluatePosition(time);
                    if (blendedPositions.find(boneName) == blendedPositions.end()) {
                        blendedPositions[boneName] = position * weight;
                    } else {
                        blendedPositions[boneName] += position * weight;
                    }
                }
                
                if (channel->hasRotationSampler()) {
                    glm::quat rotation = channel->evaluateRotation(time);
                    if (blendedRotations.find(boneName) == blendedRotations.end()) {
                        blendedRotations[boneName] = rotation * weight;
                    } else {
                        // For rotations, we need to handle quaternion blending differently
                        // This is a simplified approach - for production, consider using dual quaternions
                        glm::quat& currentRot = blendedRotations[boneName];
                        currentRot = glm::slerp(currentRot, rotation, weight / (totalWeights[boneName] + weight));
                    }
                }
                
                if (channel->hasScaleSampler()) {
                    glm::vec3 scale = channel->evaluateScale(time);
                    if (blendedScales.find(boneName) == blendedScales.end()) {
                        blendedScales[boneName] = scale * weight;
                    } else {
                        blendedScales[boneName] += scale * weight;
                    }
                }
                
                // Update total weight for this bone
                totalWeights[boneName] += weight;
            }
        }
    }
    
    // Apply blended transformations to the skeleton
    for (auto& bone : bones) {
        const std::string& boneName = bone->name;
        
        glm::mat4 transform = glm::mat4(1.0f);
        
        // Apply position
        if (blendedPositions.find(boneName) != blendedPositions.end()) {
            glm::vec3 position = blendedPositions[boneName];
            transform = glm::translate(transform, position);
        }
        
        // Apply rotation
        if (blendedRotations.find(boneName) != blendedRotations.end()) {
            glm::quat rotation = glm::normalize(blendedRotations[boneName]);
            transform = transform * glm::toMat4(rotation);
        }
        
        // Apply scale
        if (blendedScales.find(boneName) != blendedScales.end()) {
            glm::vec3 scale = blendedScales[boneName];
            transform = glm::scale(transform, scale);
        }
        
        // Update bone transform
        bone->transform = transform;
    }
    
    // Propagate changes through skeleton
    if (!bones.empty()) {
        skeleton->propegateBoneUpdate(bones[0], glm::mat4(1.0f));
    }
}

} // namespace Rapture 