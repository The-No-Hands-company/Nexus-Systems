#include <nexus/geometry/BRepFeatureStack.h>

#include <nexus/geometry/BRepBoolean.h>

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

Body FeatureStack::evaluate() const
{
    if (m_features.empty() || !m_features[0].isBase()) return Body{};

    Body cur = buildBase(m_features[0]);

    for (std::size_t k = 1; k < m_features.size(); ++k) {
        const Feature& f = m_features[k];
        Body next;
        bool applied = false;
        switch (f.kind) {
            case FeatureKind::Transform: {
                next = cur;
                applied = next.transform(f.xform);  // false on unsupported/non-finite
                break;
            }
            case FeatureKind::Chamfer: {
                next = chamferBoxEdge(cur, f.axis, f.s1, f.s2, f.amount);
                applied = next.faceCount() > 0;
                break;
            }
            case FeatureKind::Fillet: {
                next = filletBoxEdge(cur, f.axis, f.s1, f.s2, f.amount, f.segments);
                applied = next.faceCount() > 0;
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

}  // namespace nexus::geometry::brep
