#include <nexus/parametric/ConstraintGraph.h>

#include <algorithm>

namespace nexus::parametric {

ParametricEntityId ConstraintGraph::addPoint(const ParametricPoint3& point) noexcept
{
    const ParametricEntityId id = m_nextEntityId++;
    m_entities.push_back(ParametricEntity{id, point});
    return id;
}

bool ConstraintGraph::removeEntity(ParametricEntityId id) noexcept
{
    const auto it = findEntity(id);
    if (it == m_entities.end()) {
        return false;
    }

    m_entities.erase(it);

    // Keep graph validity explicit by removing constraints that referenced the erased entity.
    m_distanceConstraints.erase(
        std::remove_if(m_distanceConstraints.begin(),
                       m_distanceConstraints.end(),
                       [id](const DistanceConstraint& c) {
                           return c.entityA == id || c.entityB == id;
                       }),
        m_distanceConstraints.end());

    return true;
}

bool ConstraintGraph::hasEntity(ParametricEntityId id) const noexcept
{
    return findEntity(id) != m_entities.end();
}

bool ConstraintGraph::setPoint(ParametricEntityId id, const ParametricPoint3& point) noexcept
{
    const auto it = findEntity(id);
    if (it == m_entities.end()) {
        return false;
    }

    it->point = point;
    return true;
}

const ParametricPoint3* ConstraintGraph::point(ParametricEntityId id) const noexcept
{
    const auto it = findEntity(id);
    if (it == m_entities.end()) {
        return nullptr;
    }
    return &it->point;
}

ParametricConstraintId ConstraintGraph::addDistanceConstraint(ParametricEntityId entityA,
                                                              ParametricEntityId entityB,
                                                              double targetDistance) noexcept
{
    if (entityA == entityB || entityA == kInvalidEntityId || entityB == kInvalidEntityId ||
        targetDistance < 0.0 || !hasEntity(entityA) || !hasEntity(entityB)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_distanceConstraints.push_back(DistanceConstraint{id, entityA, entityB, targetDistance});
    return id;
}

bool ConstraintGraph::removeConstraint(ParametricConstraintId id) noexcept
{
    const auto it = findConstraint(id);
    if (it == m_distanceConstraints.end()) {
        return false;
    }

    m_distanceConstraints.erase(it);
    return true;
}

bool ConstraintGraph::hasConstraint(ParametricConstraintId id) const noexcept
{
    return findConstraint(id) != m_distanceConstraints.end();
}

std::vector<ParametricEntity>::iterator ConstraintGraph::findEntity(ParametricEntityId id) noexcept
{
    return std::find_if(m_entities.begin(),
                        m_entities.end(),
                        [id](const ParametricEntity& e) { return e.id == id; });
}

std::vector<ParametricEntity>::const_iterator ConstraintGraph::findEntity(ParametricEntityId id) const noexcept
{
    return std::find_if(m_entities.begin(),
                        m_entities.end(),
                        [id](const ParametricEntity& e) { return e.id == id; });
}

std::vector<DistanceConstraint>::iterator ConstraintGraph::findConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_distanceConstraints.begin(),
                        m_distanceConstraints.end(),
                        [id](const DistanceConstraint& c) { return c.id == id; });
}

std::vector<DistanceConstraint>::const_iterator ConstraintGraph::findConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_distanceConstraints.begin(),
                        m_distanceConstraints.end(),
                        [id](const DistanceConstraint& c) { return c.id == id; });
}

} // namespace nexus::parametric
