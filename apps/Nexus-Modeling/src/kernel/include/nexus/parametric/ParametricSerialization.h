#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — graph serialization/replay
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>

#include <string>
#include <vector>

namespace nexus::parametric {

struct ParametricSerializationReport {
    bool valid = true;
    std::vector<std::string> errors;
};

class ParametricGraphSerializer {
public:
    static std::string serialize(const ConstraintGraph& graph);

    static ParametricSerializationReport deserialize(const std::string& data,
                                                     ConstraintGraph& outGraph);
};

} // namespace nexus::parametric
