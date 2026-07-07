#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <functional>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Octree spatial index for 3D point/primitive queries.
/// Subdivides space into 8 children when capacity is exceeded.
/// Supports radius search, nearest neighbor, and frustum queries.

template <typename T>
class Octree {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct Entry { Vector3 pos; T data; };

    Octree(const Vector3& min, const Vector3& max, size_t maxPerNode = 16)
        : min_(min), max_(max), maxPerNode_(maxPerNode) {}

    void insert(const Vector3& pos, const T& data) {
        if (!contains(pos)) return;
        if (children_[0]) {
            for (int i = 0; i < 8; ++i)
                if (children_[i]->contains(pos)) { children_[i]->insert(pos, data); return; }
        }
        entries_.push_back({pos, data});
        if (entries_.size() > maxPerNode_) subdivide();
    }

    [[nodiscard]] std::vector<Entry> radiusSearch(const Vector3& center, float radius) const {
        std::vector<Entry> result;
        radiusSearchImpl(center, radius, radius * radius, result);
        return result;
    }

    [[nodiscard]] std::vector<Entry> nearestK(const Vector3& center, size_t k) const {
        std::vector<Entry> result;
        nearestKImpl(center, k, result);
        return result;
    }

    [[nodiscard]] std::vector<Entry> boxQuery(const Vector3& bmin, const Vector3& bmax) const {
        std::vector<Entry> result;
        boxQueryImpl(bmin, bmax, result);
        return result;
    }

    [[nodiscard]] size_t size() const { return entryCount(); }
    void clear() { entries_.clear(); for (auto& c : children_) c.reset(); }

private:
    Vector3 min_, max_;
    size_t maxPerNode_;
    std::vector<Entry> entries_;
    std::array<std::unique_ptr<Octree>, 8> children_;

    [[nodiscard]] bool contains(const Vector3& p) const {
        return p.x >= min_.x && p.x <= max_.x && p.y >= min_.y &&
               p.y <= max_.y && p.z >= min_.z && p.z <= max_.z;
    }

    [[nodiscard]] bool overlapsSphere(const Vector3& c, float r2) const {
        float d = 0;
        if (c.x < min_.x) d += (c.x - min_.x) * (c.x - min_.x);
        else if (c.x > max_.x) d += (c.x - max_.x) * (c.x - max_.x);
        if (c.y < min_.y) d += (c.y - min_.y) * (c.y - min_.y);
        else if (c.y > max_.y) d += (c.y - max_.y) * (c.y - max_.y);
        if (c.z < min_.z) d += (c.z - min_.z) * (c.z - min_.z);
        else if (c.z > max_.z) d += (c.z - max_.z) * (c.z - max_.z);
        return d <= r2;
    }

    [[nodiscard]] size_t entryCount() const {
        size_t n = entries_.size();
        for (auto& c : children_) if (c) n += c->entryCount();
        return n;
    }

    void subdivide() {
        Vector3 mid = {(min_.x + max_.x) * 0.5f, (min_.y + max_.y) * 0.5f, (min_.z + max_.z) * 0.5f};
        float x[2] = {min_.x, mid.x}, X[2] = {mid.x, max_.x};
        float y[2] = {min_.y, mid.y}, Y[2] = {mid.y, max_.y};
        float z[2] = {min_.z, mid.z}, Z[2] = {mid.z, max_.z};
        for (int i = 0; i < 8; ++i) {
            children_[i] = std::make_unique<Octree>(
                Vector3{x[i&1], y[(i>>1)&1], z[(i>>2)&1]},
                Vector3{X[i&1], Y[(i>>1)&1], Z[(i>>2)&1]}, maxPerNode_);
        }
        for (auto& e : entries_) {
            for (int i = 0; i < 8; ++i) {
                if (children_[i]->contains(e.pos)) { children_[i]->entries_.push_back(e); break; }
            }
        }
        entries_.clear();
    }

    void radiusSearchImpl(const Vector3& c, float r, float r2, std::vector<Entry>& out) const {
        if (!overlapsSphere(c, r2)) return;
        for (auto& e : entries_) {
            float dx = e.pos.x - c.x, dy = e.pos.y - c.y, dz = e.pos.z - c.z;
            if (dx*dx + dy*dy + dz*dz <= r2) out.push_back(e);
        }
        for (auto& ch : children_) if (ch) ch->radiusSearchImpl(c, r, r2, out);
    }

    void nearestKImpl(const Vector3& c, size_t k, std::vector<Entry>& out) const {
        for (auto& e : entries_) {
            float d2 = (e.pos.x-c.x)*(e.pos.x-c.x)+(e.pos.y-c.y)*(e.pos.y-c.y)+(e.pos.z-c.z)*(e.pos.z-c.z);
            out.push_back(e);
            std::sort(out.begin(), out.end(), [&](auto& a, auto& b) {
                float da = (a.pos.x-c.x)*(a.pos.x-c.x)+(a.pos.y-c.y)*(a.pos.y-c.y)+(a.pos.z-c.z)*(a.pos.z-c.z);
                float db = (b.pos.x-c.x)*(b.pos.x-c.x)+(b.pos.y-c.y)*(b.pos.y-c.y)+(b.pos.z-c.z)*(b.pos.z-c.z);
                return da < db;
            });
            if (out.size() > k) out.resize(k);
        }
        for (auto& ch : children_) if (ch) ch->nearestKImpl(c, k, out);
        std::sort(out.begin(), out.end(), [&](auto& a, auto& b) {
            float da = (a.pos.x-c.x)*(a.pos.x-c.x)+(a.pos.y-c.y)*(a.pos.y-c.y)+(a.pos.z-c.z)*(a.pos.z-c.z);
            float db = (b.pos.x-c.x)*(b.pos.x-c.x)+(b.pos.y-c.y)*(b.pos.y-c.y)+(b.pos.z-c.z)*(b.pos.z-c.z);
            return da < db;
        });
        if (out.size() > k) out.resize(k);
    }

    void boxQueryImpl(const Vector3& bmin, const Vector3& bmax, std::vector<Entry>& out) const {
        if (max_.x < bmin.x || min_.x > bmax.x || max_.y < bmin.y || min_.y > bmax.y ||
            max_.z < bmin.z || min_.z > bmax.z) return;
        for (auto& e : entries_) {
            if (e.pos.x >= bmin.x && e.pos.x <= bmax.x && e.pos.y >= bmin.y &&
                e.pos.y <= bmax.y && e.pos.z >= bmin.z && e.pos.z <= bmax.z)
                out.push_back(e);
        }
        for (auto& ch : children_) if (ch) ch->boxQueryImpl(bmin, bmax, out);
    }
};

} // namespace nexus::utility::graphics
