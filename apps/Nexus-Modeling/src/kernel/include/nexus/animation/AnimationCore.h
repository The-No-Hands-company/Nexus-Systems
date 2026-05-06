#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Animation — foundational deterministic animation core
//
//  Provides minimal runtime primitives for:
//  - transform keyframe sampling
//  - skeleton hierarchy ownership
//  - pose evaluation to model-space matrices
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::render {
class SceneGraph;
}

namespace nexus::animation {

struct Transform {
    nexus::render::Vec3 translation{0.f, 0.f, 0.f};
    nexus::render::Vec4 rotation{0.f, 0.f, 0.f, 1.f};
    nexus::render::Vec3 scale{1.f, 1.f, 1.f};

    static Transform identity() noexcept { return {}; }
    bool operator==(const Transform&) const = default;
};

struct TransformKeyframe {
    float     timeSec = 0.f;
    Transform value{};
};

class TransformTrack {
public:
    void setKeyframes(std::vector<TransformKeyframe> keys);
    [[nodiscard]] bool empty() const noexcept { return m_keys.empty(); }
    [[nodiscard]] Transform sample(float timeSec) const noexcept;

private:
    std::vector<TransformKeyframe> m_keys;
};

struct BoneDesc {
    std::string name;
    int32_t     parentIndex = -1;
    Transform   bindLocal{};
};

class Skeleton {
public:
    static constexpr int32_t kInvalidBone = -1;

    [[nodiscard]] int32_t addBone(BoneDesc desc);
    [[nodiscard]] size_t  boneCount() const noexcept { return m_bones.size(); }
    [[nodiscard]] int32_t parentIndex(size_t i) const noexcept;
    [[nodiscard]] const std::string& boneName(size_t i) const;
    [[nodiscard]] const Transform& bindLocal(size_t i) const;

private:
    std::vector<BoneDesc> m_bones;
};

class Pose {
public:
    explicit Pose(size_t boneCount = 0);

    void resize(size_t boneCount);
    void setLocalTransform(size_t boneIndex, const Transform& t);
    [[nodiscard]] const Transform& localTransform(size_t boneIndex) const;

    // Evaluates local transforms into model-space matrices following skeleton parent links.
    void computeModelMatrices(const Skeleton& skeleton);
    [[nodiscard]] const std::vector<nexus::render::Mat4>& modelMatrices() const noexcept {
        return m_model;
    }

private:
    std::vector<Transform>         m_local;
    std::vector<nexus::render::Mat4> m_model;
};

enum class ClipWrapMode : uint8_t {
    Clamp,
    Loop,
};

class AnimationClip {
public:
    AnimationClip(float durationSec = 0.f, float sampleRateHz = 0.f);

    void setDurationSec(float durationSec) noexcept;
    [[nodiscard]] float durationSec() const noexcept { return m_durationSec; }

    void setSampleRateHz(float sampleRateHz) noexcept;
    [[nodiscard]] float sampleRateHz() const noexcept { return m_sampleRateHz; }

    void setWrapMode(ClipWrapMode mode) noexcept { m_wrapMode = mode; }
    [[nodiscard]] ClipWrapMode wrapMode() const noexcept { return m_wrapMode; }

    void setBoneTrack(size_t boneIndex, TransformTrack track);
    void clearBoneTrack(size_t boneIndex);
    [[nodiscard]] bool hasBoneTrack(size_t boneIndex) const noexcept;
    [[nodiscard]] size_t trackCount() const noexcept;

    // Samples all bone channels into outPose; missing channels fallback to bind-local transforms.
    void sampleToPose(float timeSec, const Skeleton& skeleton, Pose& outPose) const;

private:
    [[nodiscard]] float normalizeAndQuantizeTime(float timeSec) const noexcept;

    float m_durationSec = 0.f;
    float m_sampleRateHz = 0.f;
    ClipWrapMode m_wrapMode = ClipWrapMode::Clamp;
    std::vector<std::optional<TransformTrack>> m_tracks;
};

struct BoneWeightMask {
    std::vector<float> weights;

    [[nodiscard]] float weightForBone(size_t boneIndex) const noexcept;
};

class ClipBlender {
public:
    // Blends two clips into outPose, combining global blendWeight with per-bone mask weights.
    static void blendToPose(const AnimationClip& clipA,
                            const AnimationClip& clipB,
                            float timeA,
                            float timeB,
                            float blendWeight,
                            const Skeleton& skeleton,
                            const BoneWeightMask& mask,
                            Pose& outPose);
};

enum class TransitionInterruptionPolicy : uint8_t {
    RejectNewTransition,
    QueueLastTransition,
};

class AnimationSceneBinding;

struct ClipTransitionRule {
    const AnimationClip* fromClip = nullptr;
    const AnimationClip* toClip = nullptr;
    float fadeWindowSec = 0.f;
    BoneWeightMask mask{};

    // Negative disables time predicate. Otherwise source clip local time must be >= threshold.
    float minSourceTimeSec = -1.f;

    // Empty disables event predicate. Otherwise event token must be present in queue.
    std::string eventName;
};

class ClipStateMachine {
public:
    void setInterruptionPolicy(TransitionInterruptionPolicy policy) noexcept {
        m_interruptionPolicy = policy;
    }
    [[nodiscard]] TransitionInterruptionPolicy interruptionPolicy() const noexcept {
        return m_interruptionPolicy;
    }

    void play(const AnimationClip& clip, float startTimeSec = 0.f, const BoneWeightMask& mask = {});

    // Requests cross-fade to a new clip using a deterministic fade window.
    // Returns false when request is rejected by interruption policy.
    bool requestTransition(const AnimationClip& clip,
                           float fadeWindowSec,
                           const BoneWeightMask& mask = {});

    // Advances local clip clocks and transition clock; deterministic for fixed dt streams.
    void tick(float deltaSec) noexcept;

    // Data-driven transition setup and event feed.
    void addTransitionRule(ClipTransitionRule rule);
    void clearTransitionRules() noexcept;
    void pushEvent(std::string eventName);

    [[nodiscard]] bool hasActiveClip() const noexcept { return m_currentClip != nullptr; }
    [[nodiscard]] bool isTransitioning() const noexcept { return m_nextClip != nullptr; }
    [[nodiscard]] float transitionWeight() const noexcept;
    [[nodiscard]] float currentTimeSec() const noexcept { return m_currentTimeSec; }

    // Samples either current clip or blended transition output into outPose.
    void sampleToPose(const Skeleton& skeleton, Pose& outPose) const;

    // Samples and applies output pose into scene graph using existing binding bridge.
    bool applyToSceneGraph(const Skeleton& skeleton,
                           const AnimationSceneBinding& binding,
                           nexus::render::SceneGraph& scene,
                           Pose& outPose) const;

private:
    const AnimationClip* m_currentClip = nullptr;
    float m_currentTimeSec = 0.f;

    const AnimationClip* m_nextClip = nullptr;
    float m_nextTimeSec = 0.f;
    float m_transitionElapsedSec = 0.f;
    float m_transitionWindowSec = 0.f;

    BoneWeightMask m_transitionMask{};
    TransitionInterruptionPolicy m_interruptionPolicy =
        TransitionInterruptionPolicy::RejectNewTransition;

    const AnimationClip* m_queuedClip = nullptr;
    float m_queuedTransitionWindowSec = 0.f;
    BoneWeightMask m_queuedMask{};

    std::vector<ClipTransitionRule> m_transitionRules;
    std::vector<std::string> m_pendingEvents;
};

inline constexpr uint64_t kInvalidSceneNodeId = UINT64_MAX;

class AnimationSceneBinding {
public:
    void setBoneNode(size_t boneIndex, uint64_t sceneNodeId);
    void clearBoneNode(size_t boneIndex);

    [[nodiscard]] bool hasBoneNode(size_t boneIndex) const noexcept;
    [[nodiscard]] uint64_t sceneNodeIdForBone(size_t boneIndex) const noexcept;

private:
    std::vector<uint64_t> m_boneToNode;
};

class AnimationEvaluator {
public:
    // Samples a clip and applies local transforms to bound scene nodes in deterministic bone order.
    // Returns false if any mapped scene node is missing.
    static bool applyClipToSceneGraph(const AnimationClip& clip,
                                      float timeSec,
                                      const Skeleton& skeleton,
                                      const AnimationSceneBinding& binding,
                                      nexus::render::SceneGraph& scene,
                                      Pose& outPose);

    // Blends two clips and applies local transforms to bound scene nodes in deterministic bone order.
    // Returns false if any mapped scene node is missing.
    static bool applyBlendToSceneGraph(const AnimationClip& clipA,
                                       const AnimationClip& clipB,
                                       float timeA,
                                       float timeB,
                                       float blendWeight,
                                       const BoneWeightMask& mask,
                                       const Skeleton& skeleton,
                                       const AnimationSceneBinding& binding,
                                       nexus::render::SceneGraph& scene,
                                       Pose& outPose);
};

enum class JointMatrixPackingSchema : uint8_t {
    Mat4x4RowMajorF32,
    Mat3x4RowMajorF32,
};

struct PackedJointMatrices {
    JointMatrixPackingSchema schema = JointMatrixPackingSchema::Mat4x4RowMajorF32;
    uint32_t jointCount = 0;
    uint32_t strideBytes = 0;
    std::vector<float> values;
};

class SkinningContract {
public:
    // Computes skinning matrices as modelMatrix * inverseBind for each joint.
    // Missing inverseBind entries default to identity.
    static std::vector<nexus::render::Mat4> computeJointMatrices(
        const Pose& pose,
        const std::vector<nexus::render::Mat4>& inverseBindMatrices);

    // Packs joint matrices into a deterministic linear float layout for future GPU upload wiring.
    static PackedJointMatrices packJointMatrices(
        const std::vector<nexus::render::Mat4>& jointMatrices,
        JointMatrixPackingSchema schema);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Named state graph — semantic state authoring layer over ClipStateMachine
// ─────────────────────────────────────────────────────────────────────────────

struct AnimationState {
    std::string stateId;
    const AnimationClip* clip = nullptr;
    BoneWeightMask mask{};
};

struct StateTransitionRule {
    std::string fromStateId;
    std::string toStateId;
    float fadeWindowSec = 0.f;
    BoneWeightMask mask{};

    // Negative disables time predicate. Otherwise source clip local time must be >= threshold.
    float minSourceTimeSec = -1.f;

    // Empty disables event predicate. Otherwise event token must be present in the event queue.
    std::string eventName;
};

class AnimationStateGraph {
public:
    // Registers a state. Returns false if stateId is empty, clip is null, or stateId already exists.
    bool addState(AnimationState state);

    // Adds an outgoing transition rule. Returns false if fromStateId or toStateId are not registered.
    bool addTransitionRule(StateTransitionRule rule);
    void clearTransitionRules() noexcept;

    // Starts playback of a named state. Returns false if stateId is not registered.
    bool play(const std::string& stateId, float startTimeSec = 0.f);

    void pushEvent(std::string eventName);

    // Advances the state graph; evaluates per-state outgoing rules after advancing the clip clock.
    void tick(float deltaSec) noexcept;

    [[nodiscard]] const std::string& currentStateId() const noexcept { return m_currentStateId; }
    [[nodiscard]] bool isTransitioning() const noexcept { return m_machine.isTransitioning(); }
    [[nodiscard]] float transitionWeight() const noexcept { return m_machine.transitionWeight(); }

    void sampleToPose(const Skeleton& skeleton, Pose& outPose) const;
    bool applyToSceneGraph(const Skeleton& skeleton,
                           const AnimationSceneBinding& binding,
                           nexus::render::SceneGraph& scene,
                           Pose& outPose) const;

private:
    std::unordered_map<std::string, AnimationState> m_states;
    std::vector<StateTransitionRule> m_transitionRules;
    ClipStateMachine m_machine;
    std::string m_currentStateId;
    std::string m_transitionStateId;
    std::vector<std::string> m_pendingEvents;
};

} // namespace nexus::animation
