#include <nexus/parametric/ParametricSerialization.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace nexus::parametric {

namespace {

constexpr const char* kMagic = "NEXUS_PARAM_GRAPH_V1";

bool isFiniteDouble(double value) noexcept
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
    return (bits & kExpMask) != kExpMask;
}

bool parseEntityLine(std::istringstream& line,
                     ParametricEntityId& id,
                     ParametricPoint3& point)
{
    char tag = '\0';
    line >> tag >> id >> point.x >> point.y >> point.z;
    return !line.fail() && tag == 'E' &&
           isFiniteDouble(point.x) &&
           isFiniteDouble(point.y) &&
           isFiniteDouble(point.z);
}

bool parseDistanceConstraintLine(std::istringstream& line,
                                 ParametricConstraintId& id,
                                 ParametricEntityId& a,
                                 ParametricEntityId& b,
                                 double& distance)
{
    char tag = '\0';
    std::string kind;
    line >> tag >> kind >> id >> a >> b >> distance;
    return !line.fail() && tag == 'C' && kind == "DIST" && isFiniteDouble(distance);
}

bool parseCoincidentConstraintLine(std::istringstream& line,
                                   ParametricConstraintId& id,
                                   ParametricEntityId& a,
                                   ParametricEntityId& b)
{
    char tag = '\0';
    std::string kind;
    line >> tag >> kind >> id >> a >> b;
    return !line.fail() && tag == 'C' && kind == "COINC";
}

bool parseAxisConstraintLine(std::istringstream& line,
                             ParametricConstraintId& id,
                             ParametricEntityId& a,
                             ParametricEntityId& b,
                             Axis& axis,
                             double& distance)
{
    char tag = '\0';
    std::string kind;
    char axisChar = '\0';
    line >> tag >> kind >> id >> a >> b >> axisChar >> distance;
    if (line.fail() || tag != 'C' || kind != "AXIS") {
        return false;
    }

    if (axisChar == 'X') {
        axis = Axis::X;
    } else if (axisChar == 'Y') {
        axis = Axis::Y;
    } else if (axisChar == 'Z') {
        axis = Axis::Z;
    } else {
        return false;
    }

    return isFiniteDouble(distance);
}

char axisToChar(Axis axis)
{
    if (axis == Axis::X) {
        return 'X';
    }
    if (axis == Axis::Y) {
        return 'Y';
    }
    return 'Z';
}

} // namespace

std::string ParametricGraphSerializer::serialize(const ConstraintGraph& graph)
{
    std::ostringstream out;
    out << kMagic << '\n';
    out << std::setprecision(17);

    for (const ParametricEntity& entity : graph.entities()) {
        out << "E " << entity.id << ' ' << entity.point.x << ' ' << entity.point.y << ' '
            << entity.point.z << '\n';
    }

    for (const DistanceConstraint& constraint : graph.distanceConstraints()) {
        out << "C DIST " << constraint.id << ' ' << constraint.entityA << ' '
            << constraint.entityB << ' ' << constraint.targetDistance << '\n';
    }

    for (const CoincidentConstraint& constraint : graph.coincidentConstraints()) {
        out << "C COINC " << constraint.id << ' ' << constraint.entityA << ' '
            << constraint.entityB << '\n';
    }

    for (const AxisAlignedDistanceConstraint& constraint : graph.axisAlignedDistanceConstraints()) {
        out << "C AXIS " << constraint.id << ' ' << constraint.entityA << ' '
            << constraint.entityB << ' ' << axisToChar(constraint.axis) << ' '
            << constraint.targetDistance << '\n';
    }

    return out.str();
}

ParametricSerializationReport ParametricGraphSerializer::deserialize(const std::string& data,
                                                                     ConstraintGraph& outGraph)
{
    ParametricSerializationReport report{};

    outGraph = ConstraintGraph{};

    std::istringstream input(data);
    std::string line;

    if (!std::getline(input, line) || line != kMagic) {
        report.valid = false;
        report.errors.push_back("missing or invalid serialization header");
        std::sort(report.errors.begin(), report.errors.end());
        return report;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream lineStream(line);
        const char prefix = static_cast<char>(line[0]);

        if (prefix == 'E') {
            ParametricEntityId id = kInvalidEntityId;
            ParametricPoint3 point{};
            if (!parseEntityLine(lineStream, id, point)) {
                report.valid = false;
                report.errors.push_back("failed to parse entity line: " + line);
                continue;
            }

            const ParametricEntityId inserted = outGraph.addPoint(point);
            if (inserted != id) {
                report.valid = false;
                report.errors.push_back("entity id ordering mismatch while deserializing");
                continue;
            }
        } else if (prefix == 'C') {
            {
                ParametricConstraintId id = kInvalidConstraintId;
                ParametricEntityId a = kInvalidEntityId;
                ParametricEntityId b = kInvalidEntityId;
                double distance = 0.0;
                if (parseDistanceConstraintLine(lineStream, id, a, b, distance)) {
                    const ParametricConstraintId inserted = outGraph.addDistanceConstraint(a, b, distance);
                    if (inserted != id) {
                        report.valid = false;
                        report.errors.push_back("constraint id ordering mismatch while deserializing");
                    }
                    continue;
                }
            }

            {
                std::istringstream coincidentLine(line);
                ParametricConstraintId id = kInvalidConstraintId;
                ParametricEntityId a = kInvalidEntityId;
                ParametricEntityId b = kInvalidEntityId;
                if (parseCoincidentConstraintLine(coincidentLine, id, a, b)) {
                    const ParametricConstraintId inserted = outGraph.addCoincidentConstraint(a, b);
                    if (inserted != id) {
                        report.valid = false;
                        report.errors.push_back("constraint id ordering mismatch while deserializing");
                    }
                    continue;
                }
            }

            {
                std::istringstream axisLine(line);
                ParametricConstraintId id = kInvalidConstraintId;
                ParametricEntityId a = kInvalidEntityId;
                ParametricEntityId b = kInvalidEntityId;
                Axis axis = Axis::X;
                double distance = 0.0;
                if (parseAxisConstraintLine(axisLine, id, a, b, axis, distance)) {
                    const ParametricConstraintId inserted =
                        outGraph.addAxisAlignedDistanceConstraint(a, b, axis, distance);
                    if (inserted != id) {
                        report.valid = false;
                        report.errors.push_back("constraint id ordering mismatch while deserializing");
                    }
                    continue;
                }
            }

            report.valid = false;
            report.errors.push_back("failed to parse constraint line: " + line);
        } else {
            report.valid = false;
            report.errors.push_back("unknown line type in serialization: " + line);
        }
    }

    std::sort(report.errors.begin(), report.errors.end());
    return report;
}

} // namespace nexus::parametric
