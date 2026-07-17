#include <nexus/geometry/BRepFeatureStack.h>

#include <nexus/geometry/BRepBoolean.h>

#include <bit>
#include <cstdint>
#include <utility>

namespace nexus::geometry::brep {

Feature Feature::box(float w, float h, float d)
{
    Feature f;
    f.kind = FeatureKind::Box;
    f.w = w;
    f.h = h;
    f.d = d;
    return f;
}
Feature Feature::facetedCylinder(float radius, float height, std::uint32_t segments)
{
    Feature f;
    f.kind = FeatureKind::FacetedCylinder;
    f.w = radius;
    f.h = height;
    f.segments = segments;
    return f;
}
Feature Feature::facetedSphere(float radius, std::uint32_t lat, std::uint32_t lon)
{
    Feature f;
    f.kind = FeatureKind::FacetedSphere;
    f.w = radius;
    f.lat = lat;
    f.lon = lon;
    return f;
}
Feature Feature::extrude(std::vector<Vec3> profile, Vec3 dir)
{
    Feature f;
    f.kind = FeatureKind::Extrude;
    f.profile = std::move(profile);
    f.dir = dir;
    return f;
}
Feature Feature::revolve(std::vector<Vec3> profile, Vec3 axisOrigin, Vec3 axisDir,
                         std::uint32_t segments)
{
    Feature f;
    f.kind = FeatureKind::Revolve;
    f.profile = std::move(profile);
    f.axisOrigin = axisOrigin;
    f.axisDir = axisDir;
    f.segments = segments;
    return f;
}
Feature Feature::transform(const nexus::render::Mat4& m)
{
    Feature f;
    f.kind = FeatureKind::Transform;
    f.xform = m;
    return f;
}
Feature Feature::chamfer(int axis, int s1, int s2, float setback)
{
    Feature f;
    f.kind = FeatureKind::Chamfer;
    f.axis = axis;
    f.s1 = s1;
    f.s2 = s2;
    f.amount = setback;
    return f;
}
Feature Feature::fillet(int axis, int s1, int s2, float radius, std::uint32_t segments)
{
    Feature f;
    f.kind = FeatureKind::Fillet;
    f.axis = axis;
    f.s1 = s1;
    f.s2 = s2;
    f.amount = radius;
    f.segments = segments;
    return f;
}
Feature Feature::booleanWith(BooleanOp op, std::vector<Feature> secondary)
{
    Feature f;
    f.kind = FeatureKind::Boolean;
    f.booleanOp = op;
    f.secondary = std::move(secondary);
    return f;
}

void FeatureStack::add(Feature f) { m_features.push_back(std::move(f)); }
void FeatureStack::insert(std::size_t index, Feature f)
{
    if (index > m_features.size()) index = m_features.size();
    m_features.insert(m_features.begin() + static_cast<std::ptrdiff_t>(index), std::move(f));
}
void FeatureStack::remove(std::size_t index)
{
    if (index < m_features.size())
        m_features.erase(m_features.begin() + static_cast<std::ptrdiff_t>(index));
}

namespace {
Body buildBase(const Feature& f)
{
    switch (f.kind) {
        case FeatureKind::Box:
            return makeBox(f.w, f.h, f.d);
        case FeatureKind::FacetedCylinder:
            return makeFacetedCylinder(f.w, f.h, f.segments);
        case FeatureKind::FacetedSphere:
            return makeFacetedSphere(f.w, f.lat, f.lon);
        case FeatureKind::Extrude:
            return extrudeProfile(f.profile, f.dir);
        case FeatureKind::Revolve:
            return revolveProfile(f.profile, f.axisOrigin, f.axisDir, f.segments);
        default:
            return Body{};
    }
}
}  // namespace

namespace {
// Evaluate a feature list (base + modifiers) into a solid. Shared by the
// top-level stack and each Boolean feature's secondary sub-model; `depth` bounds
// nesting so a maliciously deep tree can't overflow the stack.
Body evaluateFeatures(const std::vector<Feature>& features, int depth)
{
    if (features.empty() || !features[0].isBase() || depth > 64) return Body{};

    Body cur = buildBase(features[0]);

    for (std::size_t k = 1; k < features.size(); ++k) {
        const Feature& f = features[k];
        Body next;
        bool applied = false;
        switch (f.kind) {
            case FeatureKind::Transform:
                next = cur;
                applied = next.transform(f.xform);  // false on unsupported/non-finite
                break;
            case FeatureKind::Chamfer:
                next = chamferBoxEdge(cur, f.axis, f.s1, f.s2, f.amount);
                applied = next.faceCount() > 0;
                break;
            case FeatureKind::Fillet:
                next = filletBoxEdge(cur, f.axis, f.s1, f.s2, f.amount, f.segments);
                applied = next.faceCount() > 0;
                break;
            case FeatureKind::Boolean: {
                const Body secondary = evaluateFeatures(f.secondary, depth + 1);
                if (secondary.faceCount() > 0) {
                    next = booleanToBody(cur, secondary, f.booleanOp);
                    applied = next.faceCount() > 0;
                }
                break;
            }
            default:
                break;  // a base feature in a modifier slot is ignored
        }
        // A failed modifier leaves the running solid unchanged; never adopt an
        // integrity-failing intermediate.
        if (applied && next.checkIntegrity().ok) cur = std::move(next);
    }

    if (!cur.checkIntegrity().ok) return Body{};
    return cur;
}
}  // namespace

Body FeatureStack::evaluate() const { return evaluateFeatures(m_features, 0); }

// ──────────── Serialization ──────────────────────────────────────────────────

namespace {
constexpr std::uint32_t kStackMagic = 0x5346584Eu;  // 'NXFS'
constexpr std::uint32_t kStackVersion = 2u;  // v2 adds Boolean features (op + secondary sub-stack)

void putU8(std::vector<std::uint8_t>& o, std::uint8_t v) { o.push_back(v); }
void putU32(std::vector<std::uint8_t>& o, std::uint32_t v)
{
    o.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}
void putI32(std::vector<std::uint8_t>& o, std::int32_t v) { putU32(o, std::bit_cast<std::uint32_t>(v)); }
void putF32(std::vector<std::uint8_t>& o, float v) { putU32(o, std::bit_cast<std::uint32_t>(v)); }
void putVec3(std::vector<std::uint8_t>& o, const Vec3& v) { putF32(o, v.x); putF32(o, v.y); putF32(o, v.z); }

struct Reader {
    const std::vector<std::uint8_t>& b;
    std::size_t off = 0;
    bool ok = true;
    bool u8(std::uint8_t& out)
    {
        if (!ok || off + 1 > b.size()) return ok = false;
        out = b[off++];
        return true;
    }
    bool u32(std::uint32_t& out)
    {
        if (!ok || off + 4 > b.size()) return ok = false;
        out = static_cast<std::uint32_t>(b[off]) | (static_cast<std::uint32_t>(b[off + 1]) << 8) |
              (static_cast<std::uint32_t>(b[off + 2]) << 16) |
              (static_cast<std::uint32_t>(b[off + 3]) << 24);
        off += 4;
        return true;
    }
    bool i32(std::int32_t& out)
    {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        out = std::bit_cast<std::int32_t>(u);
        return true;
    }
    bool f32(float& out)
    {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        out = std::bit_cast<float>(u);
        if (!isFinite(out)) return ok = false;  // reject non-finite on read
        return true;
    }
    bool vec3(Vec3& out) { return f32(out.x) && f32(out.y) && f32(out.z); }
    bool count(std::uint32_t& out)  // length prefix bounded by remaining bytes
    {
        if (!u32(out)) return false;
        if (out > b.size()) return ok = false;
        return true;
    }
};

// Recursive: a Boolean feature carries a secondary sub-stack. The common record
// layout is byte-identical to v1; Boolean-only data (op + secondary list) is
// appended ONLY when kind==Boolean, so a v1 blob (which has no Boolean features)
// serializes and deserializes identically under v2.
void writeFeature(std::vector<std::uint8_t>& o, const Feature& f)
{
    putU8(o, static_cast<std::uint8_t>(f.kind));
    putF32(o, f.w);
    putF32(o, f.h);
    putF32(o, f.d);
    putF32(o, f.amount);
    putU32(o, f.segments);
    putU32(o, f.lat);
    putU32(o, f.lon);
    putI32(o, f.axis);
    putI32(o, f.s1);
    putI32(o, f.s2);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) putF32(o, f.xform.m[i][j]);
    putVec3(o, f.dir);
    putVec3(o, f.axisOrigin);
    putVec3(o, f.axisDir);
    putU32(o, static_cast<std::uint32_t>(f.profile.size()));
    for (const Vec3& p : f.profile) putVec3(o, p);
    if (f.kind == FeatureKind::Boolean) {
        putU8(o, static_cast<std::uint8_t>(f.booleanOp));
        putU32(o, static_cast<std::uint32_t>(f.secondary.size()));
        for (const Feature& s : f.secondary) writeFeature(o, s);
    }
}

bool readFeature(Reader& r, Feature& f, int depth)
{
    if (depth > 64) return false;  // bound recursion on hostile input
    std::uint8_t kind = 0;
    if (!r.u8(kind)) return false;
    if (kind > static_cast<std::uint8_t>(FeatureKind::Boolean)) return false;  // unknown kind
    f.kind = static_cast<FeatureKind>(kind);
    if (!r.f32(f.w) || !r.f32(f.h) || !r.f32(f.d) || !r.f32(f.amount)) return false;
    if (!r.u32(f.segments) || !r.u32(f.lat) || !r.u32(f.lon)) return false;
    if (!r.i32(f.axis) || !r.i32(f.s1) || !r.i32(f.s2)) return false;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!r.f32(f.xform.m[i][j])) return false;
    if (!r.vec3(f.dir) || !r.vec3(f.axisOrigin) || !r.vec3(f.axisDir)) return false;
    std::uint32_t n = 0;
    if (!r.count(n)) return false;
    f.profile.resize(n);
    for (Vec3& p : f.profile)
        if (!r.vec3(p)) return false;
    if (f.kind == FeatureKind::Boolean) {
        std::uint8_t op = 0;
        if (!r.u8(op)) return false;
        if (op > static_cast<std::uint8_t>(BooleanOp::Difference)) return false;
        f.booleanOp = static_cast<BooleanOp>(op);
        std::uint32_t sn = 0;
        if (!r.count(sn)) return false;
        f.secondary.resize(sn);
        for (Feature& s : f.secondary)
            if (!readFeature(r, s, depth + 1)) return false;
    }
    return true;
}
}  // namespace

std::vector<std::uint8_t> FeatureStack::serialize() const
{
    std::vector<std::uint8_t> o;
    putU32(o, kStackMagic);
    putU32(o, kStackVersion);
    putU32(o, static_cast<std::uint32_t>(m_features.size()));
    for (const Feature& f : m_features) writeFeature(o, f);
    return o;
}

std::optional<FeatureStack> FeatureStack::deserialize(const std::vector<std::uint8_t>& bytes)
{
    Reader r{bytes};
    std::uint32_t magic = 0, version = 0, n = 0;
    if (!r.u32(magic) || magic != kStackMagic) return std::nullopt;
    if (!r.u32(version) || version < 1u || version > kStackVersion) return std::nullopt;
    if (!r.count(n)) return std::nullopt;

    FeatureStack s;
    for (std::uint32_t i = 0; i < n; ++i) {
        Feature f;
        if (!readFeature(r, f, 0)) return std::nullopt;
        s.add(std::move(f));
    }
    if (!r.ok) return std::nullopt;
    return s;
}

}  // namespace nexus::geometry::brep
