#include <nexus/geometry/TrimBoolean.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;
using Aabb = nexus::render::Aabb;

static Aabb regionBounds(const BooleanRegion& region) {
    Aabb box;
    box.min = Vec3{std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
    box.max = Vec3{-std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max()};

    for (const auto& loop : region.outerLoops) {
        for (const auto& pt : loop.points) {
            box.min.x = std::min(box.min.x, pt.x);
            box.min.y = std::min(box.min.y, pt.y);
            box.max.x = std::max(box.max.x, pt.x);
            box.max.y = std::max(box.max.y, pt.y);
        }
    }
    for (const auto& loop : region.innerLoops) {
        for (const auto& pt : loop.points) {
            box.min.x = std::min(box.min.x, pt.x);
            box.min.y = std::min(box.min.y, pt.y);
            box.max.x = std::max(box.max.x, pt.x);
            box.max.y = std::max(box.max.y, pt.y);
        }
    }
    return box;
}

static bool pointInLoop(const Vec3& pt, const BooleanLoop& loop) {
    int32_t n = static_cast<int32_t>(loop.points.size());
    if (n < 3) return false;

    bool inside = false;
    for (int32_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec3& pi = loop.points[static_cast<size_t>(i)];
        const Vec3& pj = loop.points[static_cast<size_t>(j)];

        if (((pi.y > pt.y) != (pj.y > pt.y)) &&
            (pt.x < (pj.x - pi.x) * (pt.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

static bool pointInRegion(const Vec3& pt, const BooleanRegion& region) {
    bool inOuter = false;
    for (const auto& loop : region.outerLoops) {
        if (pointInLoop(pt, loop)) {
            inOuter = true;
            break;
        }
    }
    if (!inOuter) return false;

    for (const auto& loop : region.innerLoops) {
        if (pointInLoop(pt, loop)) return false;
    }
    return true;
}

static void rasterizeToGrid(const BooleanRegion& region,
                             const Aabb& bounds,
                             int32_t res,
                             std::vector<uint8_t>& mask) {
    mask.assign(static_cast<size_t>(res * res), 0);
    if (region.empty()) return;

    Vec3 extent = bounds.extents();
    Vec3 center = bounds.center();

    for (int32_t j = 0; j < res; ++j) {
        for (int32_t i = 0; i < res; ++i) {
            float u = (static_cast<float>(i) / static_cast<float>(res - 1) - 0.5f) * 2.0f;
            float v = (static_cast<float>(j) / static_cast<float>(res - 1) - 0.5f) * 2.0f;
            Vec3 pt{center.x + u * extent.x,
                     center.y + v * extent.y,
                     0.f};
            if (pointInRegion(pt, region)) {
                mask[static_cast<size_t>(j * res + i)] = 1;
            }
        }
    }
}

// ── Boundary extraction ──────────────────────────────────────────────────────
//
// The mask is a grid of filled/empty CELLS, and the boundary of the filled set is the
// collection of cell sides that separate a filled cell from an empty one. Emitting each
// such side as a DIRECTED edge, oriented so the filled cell lies to its left, makes every
// lattice corner have equal in- and out-degree, so the edges link head-to-tail into closed
// loops — outer boundaries counter-clockwise, holes clockwise, which is how they are told
// apart afterwards. No separate flood fill is needed to find holes: they fall out of the
// same walk with the opposite winding.
//
// WHAT THIS REPLACES, and why it matters: the previous extractBoundary() scanned the mask
// row by row and pushed the first and last cell of each horizontal RUN into a "loop". That
// is a bag of scanline span endpoints in raster order — not ordered around anything, and
// not closed. Measured on the union of two 2x2 squares, it returned the four correct
// corners in the order bottom-left, bottom-right, top-LEFT, top-right: a self-intersecting
// bowtie whose enclosed area is exactly zero. Every non-empty result this function ever
// produced had zero area, and the tests could not see it because they asserted
// `!result.empty()` and `outerLoops.size() >= 1`. It also returned a SINGLE loop, so a
// union of two disjoint regions had no way to be expressed at all.
struct GridLoops {
    std::vector<BooleanLoop> outer;
    std::vector<BooleanLoop> inner;
};

static GridLoops traceMaskBoundaries(const std::vector<uint8_t>& mask,
                                     int32_t res,
                                     const Aabb& bounds) {
    GridLoops out;
    const int32_t cw = res + 1;  // corner lattice is one wider than the cell grid
    const size_t cornerCount = static_cast<size_t>(cw) * static_cast<size_t>(cw);

    auto filled = [&](int32_t i, int32_t j) -> bool {
        if (i < 0 || i >= res || j < 0 || j >= res) return false;
        return mask[static_cast<size_t>(j) * static_cast<size_t>(res) + static_cast<size_t>(i)] != 0;
    };
    auto cornerId = [&](int32_t ci, int32_t cj) -> int32_t { return cj * cw + ci; };

    // Directed sides of every filled cell whose neighbour is empty, wound so the material
    // is on the left.
    std::vector<std::vector<int32_t>> outgoing(cornerCount);
    for (int32_t j = 0; j < res; ++j) {
        for (int32_t i = 0; i < res; ++i) {
            if (!filled(i, j)) continue;
            if (!filled(i, j - 1)) outgoing[static_cast<size_t>(cornerId(i, j))].push_back(cornerId(i + 1, j));
            if (!filled(i + 1, j)) outgoing[static_cast<size_t>(cornerId(i + 1, j))].push_back(cornerId(i + 1, j + 1));
            if (!filled(i, j + 1)) outgoing[static_cast<size_t>(cornerId(i + 1, j + 1))].push_back(cornerId(i, j + 1));
            if (!filled(i - 1, j)) outgoing[static_cast<size_t>(cornerId(i, j + 1))].push_back(cornerId(i, j));
        }
    }

    const Vec3 extent = bounds.extents();
    const Vec3 center = bounds.center();
    // A corner sits half a cell off the sample lattice, which is where the boundary
    // between a filled and an empty sample actually lies.
    auto cornerToWorld = [&](int32_t ci, int32_t cj) -> Vec3 {
        const float su = static_cast<float>(ci) - 0.5f;
        const float sv = static_cast<float>(cj) - 0.5f;
        const float u = (su / static_cast<float>(res - 1) - 0.5f) * 2.0f;
        const float v = (sv / static_cast<float>(res - 1) - 0.5f) * 2.0f;
        return Vec3{center.x + u * extent.x, center.y + v * extent.y, 0.f};
    };

    std::vector<size_t> used(cornerCount, 0);
    for (int32_t startCorner = 0; startCorner < static_cast<int32_t>(cornerCount); ++startCorner) {
        while (used[static_cast<size_t>(startCorner)] < outgoing[static_cast<size_t>(startCorner)].size()) {
            std::vector<int32_t> ring;
            int32_t cur = startCorner;
            // Follow unused outgoing edges until the walk returns to where it began. Every
            // corner has as many outgoing as incoming edges, so this always closes.
            while (true) {
                const size_t c = static_cast<size_t>(cur);
                if (used[c] >= outgoing[c].size()) break;  // defensive; should not happen
                const int32_t next = outgoing[c][used[c]];
                ++used[c];
                ring.push_back(cur);
                cur = next;
                if (cur == startCorner) break;
            }
            if (ring.size() < 4) continue;

            // Corners to world, dropping the middle of every collinear run so an
            // axis-aligned rectangle comes back as four points rather than thousands.
            std::vector<Vec3> pts;
            pts.reserve(ring.size());
            for (int32_t id : ring) pts.push_back(cornerToWorld(id % cw, id / cw));

            std::vector<Vec3> simplified;
            simplified.reserve(pts.size());
            const size_t n = pts.size();
            for (size_t k = 0; k < n; ++k) {
                const Vec3& prev = pts[(k + n - 1) % n];
                const Vec3& here = pts[k];
                const Vec3& next = pts[(k + 1) % n];
                const float ax = here.x - prev.x, ay = here.y - prev.y;
                const float bx = next.x - here.x, by = next.y - here.y;
                if (std::abs(ax * by - ay * bx) > 1e-12f) simplified.push_back(here);
            }
            if (simplified.size() < 3) continue;

            double twiceArea = 0.0;
            for (size_t k = 0; k < simplified.size(); ++k) {
                const Vec3& p = simplified[k];
                const Vec3& q = simplified[(k + 1) % simplified.size()];
                twiceArea += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
            }

            BooleanLoop loop;
            loop.closed = true;
            loop.points = std::move(simplified);
            // Counter-clockwise encloses material; clockwise is a hole.
            if (twiceArea >= 0.0) out.outer.push_back(std::move(loop));
            else out.inner.push_back(std::move(loop));
        }
    }

    return out;
}

BooleanRegion TrimBoolean::compute(
    const BooleanRegion& a,
    const BooleanRegion& b,
    BooleanOp op,
    const TrimBooleanOptions& opts) {

    BooleanRegion result;
    if (a.empty() && b.empty()) return result;

    Aabb boundsA = regionBounds(a);
    Aabb boundsB = regionBounds(b);

    Aabb combined;
    combined.min.x = std::min(boundsA.min.x, boundsB.min.x);
    combined.min.y = std::min(boundsA.min.y, boundsB.min.y);
    combined.max.x = std::max(boundsA.max.x, boundsB.max.x);
    combined.max.y = std::max(boundsA.max.y, boundsB.max.y);

    if (combined.min.x >= combined.max.x || combined.min.y >= combined.max.y) return result;

    float padX = (combined.max.x - combined.min.x) * 0.05f;
    float padY = (combined.max.y - combined.min.y) * 0.05f;
    combined.min.x -= padX;
    combined.max.x += padX;
    combined.min.y -= padY;
    combined.max.y += padY;

    int32_t res = std::max(opts.gridRes, 4);

    std::vector<uint8_t> maskA, maskB;
    rasterizeToGrid(a, combined, res, maskA);
    rasterizeToGrid(b, combined, res, maskB);

    std::vector<uint8_t> resultMask(static_cast<size_t>(res * res), 0);
    for (size_t k = 0; k < static_cast<size_t>(res * res); ++k) {
        bool inA = maskA[k] != 0;
        bool inB = maskB[k] != 0;
        bool inResult = false;
        switch (op) {
            case BooleanOp::Union:        inResult = inA || inB; break;
            case BooleanOp::Intersection: inResult = inA && inB; break;
            case BooleanOp::Difference:   inResult = inA && !inB; break;
        }
        resultMask[k] = inResult ? 1 : 0;
    }

    // One walk yields every loop, so a result with several disjoint components gets
    // several outer loops — the previous code could only ever return one.
    GridLoops loops = traceMaskBoundaries(resultMask, res, combined);
    result.outerLoops = std::move(loops.outer);
    result.innerLoops = std::move(loops.inner);

    return result;
}

} // namespace nexus::geometry
