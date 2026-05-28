#include <nexus/animation/AnimationCore.h>

#include <nexus/render/SceneGraph.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nexus::animation {

namespace {

bool isFiniteFloat(float v) noexcept
{
    return (std::bit_cast<uint32_t>(v) & 0x7F800000u) != 0x7F800000u;
}

nexus::render::Mat4 toMat4(const Transform& t)
{
    nexus::render::Transform rt{};
    rt.translation = t.translation;
    rt.rotation = t.rotation;
    rt.scale = t.scale;
    return rt.toMatrix();
}

nexus::render::Vec4 nlerp(const nexus::render::Vec4& a,
                          const nexus::render::Vec4& b,
                          float t) noexcept
{
    nexus::render::Vec4 r{};
    r.x = a.x + (b.x - a.x) * t;
    r.y = a.y + (b.y - a.y) * t;
    r.z = a.z + (b.z - a.z) * t;
    r.w = a.w + (b.w - a.w) * t;
    const float lenSq = r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w;
    if (lenSq > 1e-12f) {
        const float invLen = 1.f / std::sqrt(lenSq);
        r.x *= invLen;
        r.y *= invLen;
        r.z *= invLen;
        r.w *= invLen;
    } else {
        r = {0.f, 0.f, 0.f, 1.f};
    }
    return r;
}

// ── Quaternion helpers for spherical cubic (squad) rotation ──────────────────
using Quat = nexus::render::Vec4; // xyzw

float qdot(const Quat& a, const Quat& b) noexcept { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
Quat  qneg(const Quat& q) noexcept { return {-q.x, -q.y, -q.z, -q.w}; }
Quat  qconj(const Quat& q) noexcept { return {-q.x, -q.y, -q.z, q.w}; } // inverse for a unit quat

Quat qmul(const Quat& a, const Quat& b) noexcept
{
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
    };
}

// Spherical linear interpolation along the shortest arc; falls back to nlerp when
// the quaternions are nearly parallel (sin θ → 0).
Quat slerp(const Quat& a, Quat b, float t) noexcept
{
    float d = qdot(a, b);
    if (d < 0.f) { b = qneg(b); d = -d; } // shortest path
    if (d > 0.9995f) {
        return nlerp(a, b, t);
    }
    const float theta0 = std::acos(d);
    const float sin0   = std::sin(theta0);
    const float s0 = std::sin((1.f - t) * theta0) / sin0;
    const float s1 = std::sin(t * theta0) / sin0;
    return { s0*a.x + s1*b.x, s0*a.y + s1*b.y, s0*a.z + s1*b.z, s0*a.w + s1*b.w };
}

// Log of a unit quaternion -> pure quaternion (vector part, w = 0).
Quat qlog(const Quat& q) noexcept
{
    const float vlen = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z);
    if (vlen < 1e-8f) return {0.f, 0.f, 0.f, 0.f};
    const float a = std::atan2(vlen, q.w) / vlen;
    return {q.x*a, q.y*a, q.z*a, 0.f};
}

// Exp of a pure quaternion -> unit quaternion.
Quat qexp(const Quat& v) noexcept
{
    const float vlen = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (vlen < 1e-8f) return {0.f, 0.f, 0.f, 1.f};
    const float s = std::sin(vlen) / vlen;
    return {v.x*s, v.y*s, v.z*s, std::cos(vlen)};
}

// Shoemake squad control quaternion for the centre key q1 given neighbours q0, q2
// (already hemisphere-aligned to q1): s1 = q1 * exp(-(log(q1^-1 q2) + log(q1^-1 q0))/4).
Quat squadControl(const Quat& q0, const Quat& q1, const Quat& q2) noexcept
{
    const Quat inv = qconj(q1);
    const Quat l1 = qlog(qmul(inv, q2));
    const Quat l0 = qlog(qmul(inv, q0));
    const Quat e{ -(l0.x + l1.x) * 0.25f, -(l0.y + l1.y) * 0.25f, -(l0.z + l1.z) * 0.25f, 0.f };
    return qmul(q1, qexp(e));
}

// squad(q1,q2,s1,s2,t) = slerp(slerp(q1,q2,t), slerp(s1,s2,t), 2t(1-t)).
Quat squad(const Quat& q1, const Quat& q2, const Quat& s1, const Quat& s2, float t) noexcept
{
    return slerp(slerp(q1, q2, t), slerp(s1, s2, t), 2.f * t * (1.f - t));
}

Transform lerpTransform(const Transform& a, const Transform& b, float t) noexcept
{
    Transform out{};
    out.translation = {
        a.translation.x + (b.translation.x - a.translation.x) * t,
        a.translation.y + (b.translation.y - a.translation.y) * t,
        a.translation.z + (b.translation.z - a.translation.z) * t,
    };
    out.scale = {
        a.scale.x + (b.scale.x - a.scale.x) * t,
        a.scale.y + (b.scale.y - a.scale.y) * t,
        a.scale.z + (b.scale.z - a.scale.z) * t,
    };
    out.rotation = nlerp(a.rotation, b.rotation, t);
    return out;
}

// Catmull-Rom cubic Hermite through p1->p2 with auto tangents from neighbours
// p0 and p3 (tangents m1 = (p2-p0)/2, m2 = (p3-p1)/2). Endpoints clamp to a
// neighbour so the tangent degrades gracefully to one-sided.
nexus::render::Vec3 catmullRom(const nexus::render::Vec3& p0, const nexus::render::Vec3& p1,
                               const nexus::render::Vec3& p2, const nexus::render::Vec3& p3,
                               float t) noexcept
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float h00 =  2.f*t3 - 3.f*t2 + 1.f;
    const float h10 =       t3 - 2.f*t2 + t;
    const float h01 = -2.f*t3 + 3.f*t2;
    const float h11 =       t3 -     t2;
    auto comp = [&](float a0, float a1, float a2, float a3) {
        const float m1 = (a2 - a0) * 0.5f;
        const float m2 = (a3 - a1) * 0.5f;
        return h00*a1 + h10*m1 + h01*a2 + h11*m2;
    };
    return { comp(p0.x, p1.x, p2.x, p3.x),
             comp(p0.y, p1.y, p2.y, p3.y),
             comp(p0.z, p1.z, p2.z, p3.z) };
}

float clamp01(float v) noexcept
{
    return std::clamp(v, 0.f, 1.f);
}

render::Transform toRenderTransform(const Transform& t) noexcept
{
    render::Transform rt{};
    rt.translation = t.translation;
    rt.rotation = t.rotation;
    rt.scale = t.scale;
    return rt;
}

bool applyPoseToSceneGraph(const Pose& pose,
                           const AnimationSceneBinding& binding,
                           render::SceneGraph& scene,
                           size_t boneCount)
{
    bool ok = true;
    for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
        if (!binding.hasBoneNode(boneIndex)) {
            continue;
        }

        const uint64_t nodeId = binding.sceneNodeIdForBone(boneIndex);
        auto* node = scene.findNode(nodeId);
        if (!node) {
            ok = false;
            continue;
        }

        node->localTransform() = toRenderTransform(pose.localTransform(boneIndex));
        node->markDirty();
    }
    return ok;
}

int findEventIndex(const std::vector<std::string>& events, const std::string& token)
{
    if (token.empty()) {
        return -1;
    }

    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i] == token) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

void TransformTrack::setKeyframes(std::vector<TransformKeyframe> keys)
{
    std::sort(keys.begin(), keys.end(),
              [](const TransformKeyframe& a, const TransformKeyframe& b) {
                  return a.timeSec < b.timeSec;
              });
    m_keys = std::move(keys);
}

const TransformKeyframe* TransformTrack::keyframe(size_t index) const noexcept
{
    if (index >= m_keys.size()) {
        return nullptr;
    }
    return &m_keys[index];
}

Transform TransformTrack::sample(float timeSec) const noexcept
{
    if (m_keys.empty()) return Transform::identity();
    if (timeSec <= m_keys.front().timeSec) return m_keys.front().value;
    if (timeSec >= m_keys.back().timeSec) return m_keys.back().value;

    for (size_t i = 1; i < m_keys.size(); ++i) {
        const auto& prev = m_keys[i - 1];
        const auto& next = m_keys[i];
        if (timeSec <= next.timeSec) {
            const float dt = next.timeSec - prev.timeSec;
            if (dt <= 1e-8f) return next.value;
            const float t = (timeSec - prev.timeSec) / dt;

            // The segment's interpolation is chosen by its left (prev) keyframe.
            switch (prev.interpolation) {
                case KeyInterpolation::Step:
                    return prev.value; // hold until the next keyframe

                case KeyInterpolation::Cubic: {
                    // Auto-tangent Catmull-Rom over translation/scale, and a proper
                    // spherical cubic (squad) over rotation using the surrounding keys
                    // as the spline control points.
                    const Transform& p0 = (i >= 2) ? m_keys[i - 2].value : prev.value;
                    const Transform& p3 = (i + 1 < m_keys.size()) ? m_keys[i + 1].value : next.value;
                    Transform out{};
                    out.translation = catmullRom(p0.translation, prev.value.translation,
                                                 next.value.translation, p3.translation, t);
                    out.scale       = catmullRom(p0.scale, prev.value.scale,
                                                 next.value.scale, p3.scale, t);

                    // Hemisphere-align the four control quaternions into one chain so
                    // the spline (and its log/exp tangents) take the shortest path.
                    const Quat q1 = prev.value.rotation;
                    Quat q0 = p0.rotation, q2 = next.value.rotation, q3 = p3.rotation;
                    if (qdot(q1, q0) < 0.f) q0 = qneg(q0);
                    if (qdot(q1, q2) < 0.f) q2 = qneg(q2);
                    if (qdot(q2, q3) < 0.f) q3 = qneg(q3);
                    const Quat s1 = squadControl(q0, q1, q2);
                    const Quat s2 = squadControl(q1, q2, q3);
                    out.rotation = squad(q1, q2, s1, s2, t);
                    return out;
                }

                case KeyInterpolation::Linear:
                default:
                    return lerpTransform(prev.value, next.value, t);
            }
        }
    }

    return m_keys.back().value;
}

int32_t Skeleton::addBone(BoneDesc desc)
{
    if (desc.name.empty()) return kInvalidBone;
    if (desc.parentIndex < -1) return kInvalidBone;
    if (desc.parentIndex >= static_cast<int32_t>(m_bones.size())) return kInvalidBone;

    m_bones.push_back(std::move(desc));
    return static_cast<int32_t>(m_bones.size() - 1);
}

int32_t Skeleton::parentIndex(size_t i) const noexcept
{
    if (i >= m_bones.size()) return kInvalidBone;
    return m_bones[i].parentIndex;
}

const std::string& Skeleton::boneName(size_t i) const
{
    return m_bones.at(i).name;
}

int32_t Skeleton::findBoneIndexByName(const std::string& name) const noexcept
{
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_bones[i].name == name) {
            return static_cast<int32_t>(i);
        }
    }
    return kInvalidBone;
}

const Transform& Skeleton::bindLocal(size_t i) const
{
    return m_bones.at(i).bindLocal;
}

Pose::Pose(size_t boneCount)
    : m_local(boneCount, Transform::identity()),
      m_model(boneCount, nexus::render::Mat4::identity())
{}

void Pose::resize(size_t boneCount)
{
    m_local.resize(boneCount, Transform::identity());
    m_model.resize(boneCount, nexus::render::Mat4::identity());
}

void Pose::setLocalTransform(size_t boneIndex, const Transform& t)
{
    if (boneIndex >= m_local.size()) return;
    m_local[boneIndex] = t;
}

const Transform& Pose::localTransform(size_t boneIndex) const
{
    return m_local.at(boneIndex);
}

void Pose::computeModelMatrices(const Skeleton& skeleton)
{
    if (m_local.size() != skeleton.boneCount()) {
        resize(skeleton.boneCount());
    }

    for (size_t i = 0; i < m_local.size(); ++i) {
        const auto localM = toMat4(m_local[i]);
        const int32_t parent = skeleton.parentIndex(i);
        if (parent >= 0) {
            m_model[i] = m_model[static_cast<size_t>(parent)] * localM;
        } else {
            m_model[i] = localM;
        }
    }
}

void NotifyTrack::setEvents(std::vector<AnimationEvent> events)
{
    std::sort(events.begin(), events.end(),
              [](const AnimationEvent& a, const AnimationEvent& b) {
                  if (a.timeSec != b.timeSec) return a.timeSec < b.timeSec;
                  return a.id < b.id; // stable, deterministic order for equal times
              });
    m_events = std::move(events);
}

void NotifyTrack::collectEvents(float fromTime, float toTime, std::vector<AnimationEvent>& out) const
{
    for (const AnimationEvent& e : m_events) {
        if (e.timeSec > fromTime && e.timeSec <= toTime) {
            out.push_back(e);
        }
    }
}

AnimationClip::AnimationClip(float durationSec, float sampleRateHz)
    : m_durationSec(isFiniteFloat(durationSec) ? std::max(0.f, durationSec) : 0.f),
      m_sampleRateHz(isFiniteFloat(sampleRateHz) ? std::max(0.f, sampleRateHz) : 0.f)
{}

void AnimationClip::setNotifyTrack(NotifyTrack track)
{
    m_notifies = std::move(track);
}

void AnimationClip::collectEvents(float fromTime, float toTime, std::vector<AnimationEvent>& out) const
{
    m_notifies.collectEvents(fromTime, toTime, out);
}

void AnimationClip::setDurationSec(float durationSec) noexcept
{
    if (!isFiniteFloat(durationSec) || durationSec < 0.f) return;
    m_durationSec = durationSec;
}

void AnimationClip::setSampleRateHz(float sampleRateHz) noexcept
{
    if (!isFiniteFloat(sampleRateHz) || sampleRateHz < 0.f) return;
    m_sampleRateHz = sampleRateHz;
}

void AnimationClip::setBoneTrack(size_t boneIndex, TransformTrack track)
{
    if (boneIndex >= m_tracks.size()) {
        m_tracks.resize(boneIndex + 1);
    }
    m_tracks[boneIndex] = std::move(track);
}

void AnimationClip::clearBoneTrack(size_t boneIndex)
{
    if (boneIndex >= m_tracks.size()) return;
    m_tracks[boneIndex].reset();
}

bool AnimationClip::hasBoneTrack(size_t boneIndex) const noexcept
{
    return boneIndex < m_tracks.size() && m_tracks[boneIndex].has_value();
}

const TransformTrack* AnimationClip::boneTrack(size_t boneIndex) const noexcept
{
    if (!hasBoneTrack(boneIndex)) {
        return nullptr;
    }
    return &m_tracks[boneIndex].value();
}

size_t AnimationClip::trackCount() const noexcept
{
    size_t count = 0;
    for (const auto& track : m_tracks) {
        if (track.has_value()) {
            ++count;
        }
    }
    return count;
}

void AnimationClip::sampleToPose(float timeSec, const Skeleton& skeleton, Pose& outPose) const
{
    if (outPose.modelMatrices().size() != skeleton.boneCount()) {
        outPose.resize(skeleton.boneCount());
    }

    const float sampledTime = normalizeAndQuantizeTime(timeSec);
    for (size_t boneIndex = 0; boneIndex < skeleton.boneCount(); ++boneIndex) {
        if (hasBoneTrack(boneIndex)) {
            outPose.setLocalTransform(boneIndex, m_tracks[boneIndex]->sample(sampledTime));
        } else {
            outPose.setLocalTransform(boneIndex, skeleton.bindLocal(boneIndex));
        }
    }
}

float AnimationClip::normalizeAndQuantizeTime(float timeSec) const noexcept
{
    float t = timeSec;

    if (m_durationSec > 0.f) {
        if (m_wrapMode == ClipWrapMode::Loop) {
            t = std::fmod(timeSec, m_durationSec);
            if (t < 0.f) {
                t += m_durationSec;
            }
        } else {
            t = std::clamp(timeSec, 0.f, m_durationSec);
        }
    }

    if (m_sampleRateHz > std::numeric_limits<float>::epsilon()) {
        const float samples = t * m_sampleRateHz;
        t = std::round(samples) / m_sampleRateHz;
    }

    return t;
}

float BoneWeightMask::weightForBone(size_t boneIndex) const noexcept
{
    if (boneIndex >= weights.size()) {
        return 1.f;
    }
    return clamp01(weights[boneIndex]);
}

void ClipBlender::blendToPose(const AnimationClip& clipA,
                              const AnimationClip& clipB,
                              float timeA,
                              float timeB,
                              float blendWeight,
                              const Skeleton& skeleton,
                              const BoneWeightMask& mask,
                              Pose& outPose)
{
    Pose poseA(skeleton.boneCount());
    Pose poseB(skeleton.boneCount());
    clipA.sampleToPose(timeA, skeleton, poseA);
    clipB.sampleToPose(timeB, skeleton, poseB);

    outPose.resize(skeleton.boneCount());
    const float globalBlend = clamp01(blendWeight);

    for (size_t boneIndex = 0; boneIndex < skeleton.boneCount(); ++boneIndex) {
        const float boneBlend = clamp01(globalBlend * mask.weightForBone(boneIndex));
        outPose.setLocalTransform(
            boneIndex,
            lerpTransform(
                poseA.localTransform(boneIndex),
                poseB.localTransform(boneIndex),
                boneBlend));
    }
}

void ClipStateMachine::play(const AnimationClip& clip,
                            float startTimeSec,
                            const BoneWeightMask& mask)
{
    m_currentClip = &clip;
    m_currentTimeSec = isFiniteFloat(startTimeSec) ? std::max(0.f, startTimeSec) : 0.f;

    m_nextClip = nullptr;
    m_nextTimeSec = 0.f;
    m_transitionElapsedSec = 0.f;
    m_transitionWindowSec = 0.f;
    m_transitionMask = mask;

    m_queuedClip = nullptr;
    m_queuedTransitionWindowSec = 0.f;
    m_queuedMask = {};
}

bool ClipStateMachine::requestTransition(const AnimationClip& clip,
                                         float fadeWindowSec,
                                         const BoneWeightMask& mask)
{
    const float fade = isFiniteFloat(fadeWindowSec) ? std::max(0.f, fadeWindowSec) : 0.f;

    if (!m_currentClip) {
        play(clip, 0.f, mask);
        return true;
    }

    if (m_nextClip) {
        if (m_interruptionPolicy == TransitionInterruptionPolicy::RejectNewTransition) {
            return false;
        }
        m_queuedClip = &clip;
        m_queuedTransitionWindowSec = fade;
        m_queuedMask = mask;
        return true;
    }

    if (fade <= std::numeric_limits<float>::epsilon()) {
        play(clip, 0.f, mask);
        return true;
    }

    m_nextClip = &clip;
    m_nextTimeSec = 0.f;
    m_transitionElapsedSec = 0.f;
    m_transitionWindowSec = fade;
    m_transitionMask = mask;
    return true;
}

void ClipStateMachine::tick(float deltaSec) noexcept
{
    const float dt = isFiniteFloat(deltaSec) ? std::max(0.f, deltaSec) : 0.f;
    if (!m_currentClip) {
        return;
    }

    m_currentTimeSec += dt;

    if (m_nextClip) {
        m_nextTimeSec += dt;
        m_transitionElapsedSec += dt;

        if (m_transitionElapsedSec + std::numeric_limits<float>::epsilon() >= m_transitionWindowSec) {
            m_currentClip = m_nextClip;
            m_currentTimeSec = m_nextTimeSec;

            m_nextClip = nullptr;
            m_nextTimeSec = 0.f;
            m_transitionElapsedSec = 0.f;
            m_transitionWindowSec = 0.f;

            if (m_queuedClip) {
                const AnimationClip* queuedClip = m_queuedClip;
                const float queuedWindow = m_queuedTransitionWindowSec;
                const BoneWeightMask queuedMask = m_queuedMask;

                m_queuedClip = nullptr;
                m_queuedTransitionWindowSec = 0.f;
                m_queuedMask = {};

                (void)requestTransition(*queuedClip, queuedWindow, queuedMask);
            }
        }
    }

    if (!m_currentClip) {
        return;
    }

    for (const auto& rule : m_transitionRules) {
        if (!rule.fromClip || !rule.toClip) {
            continue;
        }
        if (rule.fromClip != m_currentClip) {
            continue;
        }

        if (rule.minSourceTimeSec >= 0.f && m_currentTimeSec < rule.minSourceTimeSec) {
            continue;
        }

        int eventIndex = -1;
        if (!rule.eventName.empty()) {
            eventIndex = findEventIndex(m_pendingEvents, rule.eventName);
            if (eventIndex < 0) {
                continue;
            }
        }

        if (requestTransition(*rule.toClip, rule.fadeWindowSec, rule.mask)) {
            if (eventIndex >= 0) {
                m_pendingEvents.erase(m_pendingEvents.begin() + eventIndex);
            }
            break;
        }
    }
}

void ClipStateMachine::addTransitionRule(ClipTransitionRule rule)
{
    m_transitionRules.push_back(std::move(rule));
}

void ClipStateMachine::clearTransitionRules() noexcept
{
    m_transitionRules.clear();
}

void ClipStateMachine::pushEvent(std::string eventName)
{
    if (eventName.empty()) {
        return;
    }
    m_pendingEvents.push_back(std::move(eventName));
}

float ClipStateMachine::transitionWeight() const noexcept
{
    if (!m_nextClip) {
        return 0.f;
    }
    if (m_transitionWindowSec <= std::numeric_limits<float>::epsilon()) {
        return 1.f;
    }
    return clamp01(m_transitionElapsedSec / m_transitionWindowSec);
}

void ClipStateMachine::sampleToPose(const Skeleton& skeleton, Pose& outPose) const
{
    if (!m_currentClip) {
        outPose.resize(skeleton.boneCount());
        for (size_t boneIndex = 0; boneIndex < skeleton.boneCount(); ++boneIndex) {
            outPose.setLocalTransform(boneIndex, skeleton.bindLocal(boneIndex));
        }
        return;
    }

    if (!m_nextClip) {
        m_currentClip->sampleToPose(m_currentTimeSec, skeleton, outPose);
        return;
    }

    ClipBlender::blendToPose(*m_currentClip,
                             *m_nextClip,
                             m_currentTimeSec,
                             m_nextTimeSec,
                             transitionWeight(),
                             skeleton,
                             m_transitionMask,
                             outPose);
}

bool ClipStateMachine::applyToSceneGraph(const Skeleton& skeleton,
                                         const AnimationSceneBinding& binding,
                                         nexus::render::SceneGraph& scene,
                                         Pose& outPose) const
{
    sampleToPose(skeleton, outPose);
    outPose.computeModelMatrices(skeleton);
    return applyPoseToSceneGraph(outPose, binding, scene, skeleton.boneCount());
}

void AnimationSceneBinding::setBoneNode(size_t boneIndex, uint64_t sceneNodeId)
{
    if (boneIndex >= m_boneToNode.size()) {
        m_boneToNode.resize(boneIndex + 1, kInvalidSceneNodeId);
    }
    m_boneToNode[boneIndex] = sceneNodeId;
}

void AnimationSceneBinding::clearBoneNode(size_t boneIndex)
{
    if (boneIndex >= m_boneToNode.size()) {
        return;
    }
    m_boneToNode[boneIndex] = kInvalidSceneNodeId;
}

bool AnimationSceneBinding::hasBoneNode(size_t boneIndex) const noexcept
{
    return sceneNodeIdForBone(boneIndex) != kInvalidSceneNodeId;
}

uint64_t AnimationSceneBinding::sceneNodeIdForBone(size_t boneIndex) const noexcept
{
    if (boneIndex >= m_boneToNode.size()) {
        return kInvalidSceneNodeId;
    }
    return m_boneToNode[boneIndex];
}

bool AnimationEvaluator::applyClipToSceneGraph(const AnimationClip& clip,
                                               float timeSec,
                                               const Skeleton& skeleton,
                                               const AnimationSceneBinding& binding,
                                               render::SceneGraph& scene,
                                               Pose& outPose)
{
    clip.sampleToPose(timeSec, skeleton, outPose);
    outPose.computeModelMatrices(skeleton);
    return applyPoseToSceneGraph(outPose, binding, scene, skeleton.boneCount());
}

bool AnimationEvaluator::applyBlendToSceneGraph(const AnimationClip& clipA,
                                                const AnimationClip& clipB,
                                                float timeA,
                                                float timeB,
                                                float blendWeight,
                                                const BoneWeightMask& mask,
                                                const Skeleton& skeleton,
                                                const AnimationSceneBinding& binding,
                                                render::SceneGraph& scene,
                                                Pose& outPose)
{
    ClipBlender::blendToPose(clipA, clipB, timeA, timeB, blendWeight, skeleton, mask, outPose);
    outPose.computeModelMatrices(skeleton);
    return applyPoseToSceneGraph(outPose, binding, scene, skeleton.boneCount());
}

std::vector<nexus::render::Mat4> SkinningContract::computeJointMatrices(
    const Pose& pose,
    const std::vector<nexus::render::Mat4>& inverseBindMatrices)
{
    const auto& model = pose.modelMatrices();
    std::vector<nexus::render::Mat4> out(model.size(), nexus::render::Mat4::identity());

    for (size_t i = 0; i < model.size(); ++i) {
        const nexus::render::Mat4 invBind =
            (i < inverseBindMatrices.size()) ? inverseBindMatrices[i] : nexus::render::Mat4::identity();
        out[i] = model[i] * invBind;
    }
    return out;
}

PackedJointMatrices SkinningContract::packJointMatrices(
    const std::vector<nexus::render::Mat4>& jointMatrices,
    JointMatrixPackingSchema schema)
{
    PackedJointMatrices packed{};
    packed.schema = schema;
    packed.jointCount = static_cast<uint32_t>(jointMatrices.size());

    if (schema == JointMatrixPackingSchema::Mat3x4RowMajorF32) {
        packed.strideBytes = static_cast<uint32_t>(12 * sizeof(float));
        packed.values.resize(jointMatrices.size() * 12);

        size_t dst = 0;
        for (const auto& m : jointMatrices) {
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 4; ++col) {
                    packed.values[dst++] = m.m[row][col];
                }
            }
        }
        return packed;
    }

    packed.strideBytes = static_cast<uint32_t>(16 * sizeof(float));
    packed.values.resize(jointMatrices.size() * 16);

    size_t dst = 0;
    for (const auto& m : jointMatrices) {
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                packed.values[dst++] = m.m[row][col];
            }
        }
    }
    return packed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationStateGraph
// ─────────────────────────────────────────────────────────────────────────────

bool AnimationStateGraph::addState(AnimationState state)
{
    if (state.stateId.empty() || !state.clip) {
        return false;
    }
    if (m_states.count(state.stateId)) {
        return false;
    }
    m_states.emplace(state.stateId, std::move(state));
    return true;
}

bool AnimationStateGraph::addTransitionRule(StateTransitionRule rule)
{
    if (!m_states.count(rule.fromStateId) || !m_states.count(rule.toStateId)) {
        return false;
    }
    m_transitionRules.push_back(std::move(rule));
    return true;
}

void AnimationStateGraph::clearTransitionRules() noexcept
{
    m_transitionRules.clear();
}

bool AnimationStateGraph::play(const std::string& stateId, float startTimeSec)
{
    if (!isFiniteFloat(startTimeSec) || startTimeSec < 0.f) {
        return false;
    }

    auto it = m_states.find(stateId);
    if (it == m_states.end() || !it->second.clip) {
        return false;
    }
    m_machine.play(*it->second.clip, startTimeSec, it->second.mask);
    m_currentStateId = stateId;
    m_transitionStateId.clear();
    return true;
}

void AnimationStateGraph::pushEvent(std::string eventName)
{
    if (eventName.empty()) {
        return;
    }
    m_pendingEvents.push_back(std::move(eventName));
}

void AnimationStateGraph::tick(float deltaSec) noexcept
{
    if (!isFiniteFloat(deltaSec) || deltaSec < 0.f) {
        return;
    }

    m_machine.tick(deltaSec);

    // Promote the pending transition state to current once the cross-fade completes.
    if (!m_transitionStateId.empty() && !m_machine.isTransitioning()) {
        m_currentStateId = std::move(m_transitionStateId);
        m_transitionStateId.clear();
    }

    if (m_currentStateId.empty()) {
        return;
    }

    // Evaluate per-state outgoing rules.
    for (const auto& rule : m_transitionRules) {
        if (rule.fromStateId != m_currentStateId) {
            continue;
        }

        if (rule.minSourceTimeSec >= 0.f && m_machine.currentTimeSec() < rule.minSourceTimeSec) {
            continue;
        }

        int eventIndex = -1;
        if (!rule.eventName.empty()) {
            for (size_t i = 0; i < m_pendingEvents.size(); ++i) {
                if (m_pendingEvents[i] == rule.eventName) {
                    eventIndex = static_cast<int>(i);
                    break;
                }
            }
            if (eventIndex < 0) {
                continue;
            }
        }

        auto it = m_states.find(rule.toStateId);
        if (it == m_states.end() || !it->second.clip) {
            continue;
        }

        if (m_machine.requestTransition(*it->second.clip, rule.fadeWindowSec, rule.mask)) {
            m_transitionStateId = rule.toStateId;
            if (eventIndex >= 0) {
                m_pendingEvents.erase(m_pendingEvents.begin() + eventIndex);
            }
            // For instant transitions (fadeWindowSec=0) the machine is already past the
            // cross-fade; promote the state ID immediately rather than waiting for the next tick.
            if (!m_machine.isTransitioning()) {
                m_currentStateId = std::move(m_transitionStateId);
                m_transitionStateId.clear();
            }
            break;
        }
    }
}

void AnimationStateGraph::sampleToPose(const Skeleton& skeleton, Pose& outPose) const
{
    m_machine.sampleToPose(skeleton, outPose);
}

bool AnimationStateGraph::applyToSceneGraph(const Skeleton& skeleton,
                                             const AnimationSceneBinding& binding,
                                             nexus::render::SceneGraph& scene,
                                             Pose& outPose) const
{
    return m_machine.applyToSceneGraph(skeleton, binding, scene, outPose);
}

} // namespace nexus::animation
