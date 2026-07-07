#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <stack>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <random>

namespace nexus::utility::procgen {

/// L-System parser, debugger, and generation visualizer
class LSystemDebugger {
public:
    struct Rule {
        char predecessor;
        std::string successor;
        double probability = 1.0;  // for stochastic L-systems
    };

    struct LSystem {
        std::string axiom;
        std::vector<Rule> rules;
        int iterations = 4;
        double angle = 25.0;         // degrees for turtle graphics
        double step_length = 1.0;
        double scale_factor = 0.5;   // for growth simulation
    };

    struct GenerationResult {
        std::string string;
        std::vector<std::string> history; // each iteration's output
        struct Stats {
            size_t length = 0;
            size_t branches = 0;         // '[' count
            size_t leaves = 0;           // count of terminal symbols
            double growth_rate = 0;      // length ratio between iterations
            std::map<char, size_t> symbol_counts;
        } stats;
    };

    struct TurtlePath {
        std::vector<double> x, y;        // path coordinates
        std::vector<bool> draw;          // whether segment is drawn
        std::vector<int> depth;          // branch depth
    };

    /// Parse an L-system specification string
    static LSystem parse(std::string_view spec) {
        LSystem ls;
        std::string specStr(spec);
        std::istringstream iss(specStr);
        std::string line;

        while (std::getline(iss, line)) {
            // Trim
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            if (trimmed.starts_with("axiom:")) {
                ls.axiom = trim(trimmed.substr(6));
            } else if (trimmed.starts_with("angle:")) {
                ls.angle = std::stod(trim(trimmed.substr(6)));
            } else if (trimmed.starts_with("iterations:")) {
                ls.iterations = std::stoi(trim(trimmed.substr(11)));
            } else if (trimmed.starts_with("step:")) {
                ls.step_length = std::stod(trim(trimmed.substr(5)));
            } else if (trimmed.find("->") != std::string::npos) {
                auto arrow = trimmed.find("->");
                char pred = trimmed[0];
                std::string succ = trim(trimmed.substr(arrow + 2));
                double prob = 1.0;

                // Check for probability notation: F(0.5)->...
                if (trimmed.size() > 2 && trimmed[1] == '(') {
                    auto paren = trimmed.find(')');
                    if (paren != std::string::npos) {
                        prob = std::stod(trim(trimmed.substr(2, paren - 2)));
                        succ = trim(trimmed.substr(arrow + 2));
                    }
                }
                ls.rules.push_back({pred, succ, prob});
            }
        }

        return ls;
    }

    /// Generate L-system string for N iterations
    static GenerationResult generate(const LSystem& ls) {
        GenerationResult result;
        std::string current = ls.axiom;
        result.history.push_back(current);

        std::mt19937_64 rng(42); // deterministic seed

        for (int iter = 0; iter < ls.iterations; ++iter) {
            std::string next;
            for (char c : current) {
                // Find matching rules
                std::vector<const Rule*> matching;
                for (auto& rule : ls.rules) {
                    if (rule.predecessor == c) matching.push_back(&rule);
                }

                if (matching.empty()) {
                    next += c; // identity rule
                } else if (matching.size() == 1 || matching[0]->probability >= 1.0) {
                    next += matching[0]->successor;
                } else {
                    // Stochastic selection
                    double r = std::uniform_real_distribution<double>(0, 1)(rng);
                    double cumulative = 0;
                    bool found = false;
                    for (auto* rule : matching) {
                        cumulative += rule->probability;
                        if (r <= cumulative) {
                            next += rule->successor;
                            found = true;
                            break;
                        }
                    }
                    if (!found) next += matching.back()->successor;
                }
            }
            current = next;
            result.history.push_back(current);
        }

        result.string = current;
        computeStats(result);
        return result;
    }

    /// Generate turtle graphics path from L-system string
    static TurtlePath generateTurtlePath(const std::string& lstring,
                                          double angle_deg, double step = 1.0) {
        TurtlePath path;
        double x = 0, y = 0, dir = 90; // start pointing up
        path.x.push_back(x);
        path.y.push_back(y);
        path.draw.push_back(false);
        path.depth.push_back(0);

        struct State { double x, y, dir; };
        std::stack<State> stack;
        int depth = 0;

        for (char c : lstring) {
            switch (c) {
                case 'F':
                case 'G':
                case 'A':
                case 'B': {
                    double rad = dir * M_PI / 180.0;
                    x += std::cos(rad) * step;
                    y += std::sin(rad) * step;
                    path.x.push_back(x);
                    path.y.push_back(y);
                    path.draw.push_back(true);
                    path.depth.push_back(depth);
                    break;
                }
                case 'f':
                case 'g': {
                    double rad = dir * M_PI / 180.0;
                    x += std::cos(rad) * step;
                    y += std::sin(rad) * step;
                    path.x.push_back(x);
                    path.y.push_back(y);
                    path.draw.push_back(false);
                    path.depth.push_back(depth);
                    break;
                }
                case '+': dir += angle_deg; break;
                case '-': dir -= angle_deg; break;
                case '|': dir += 180; break;
                case '[':
                    stack.push({x, y, dir});
                    depth++;
                    break;
                case ']':
                    if (!stack.empty()) {
                        auto s = stack.top(); stack.pop();
                        x = s.x; y = s.y; dir = s.dir;
                        path.x.push_back(x);
                        path.y.push_back(y);
                        path.draw.push_back(false);
                        path.depth.push_back(depth);
                        depth--;
                    }
                    break;
                default: break;
            }
        }

        return path;
    }

    /// Validate an L-system for common issues
    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> warnings;
        bool has_brackets = false;
        bool has_balanced_brackets = true;
        bool has_draw_commands = false;
        bool has_turn_commands = false;
        double estimated_redundancy = 0; // repetitive pattern score
    };

    static ValidationResult validate(const LSystem& ls, const std::string& output) {
        ValidationResult vr;

        // Check bracket balance
        int depth = 0;
        for (char c : output) {
            if (c == '[') { depth++; vr.has_brackets = true; }
            if (c == ']') depth--;
            if (depth < 0) { vr.has_balanced_brackets = false; break; }
            if (c == 'F' || c == 'G' || c == 'A') vr.has_draw_commands = true;
            if (c == '+' || c == '-') vr.has_turn_commands = true;
        }
        if (depth != 0) vr.has_balanced_brackets = false;

        if (!vr.has_draw_commands)
            vr.warnings.push_back("No draw commands (F/G/A) in output");
        if (!vr.has_balanced_brackets)
            vr.warnings.push_back("Unbalanced branch brackets [ ]");
        if (output.size() > 100000)
            vr.warnings.push_back("Very large output string (>100K chars)");

        // Estimate redundancy: compressibility metric
        vr.estimated_redundancy = estimateRedundancy(output);

        vr.valid = vr.warnings.empty();
        return vr;
    }

    /// Generate readable report
    static std::string report(const GenerationResult& gen,
                               const ValidationResult& val) {
        std::ostringstream oss;
        oss << "═══ L-System Debug Report ═══\n";
        oss << "  Iterations:       " << (gen.history.size() - 1) << "\n";
        oss << "  Final Length:     " << gen.stats.length << "\n";
        oss << "  Branches:         " << gen.stats.branches << "\n";
        oss << "  Growth Rate:      " << gen.stats.growth_rate << "\n";
        oss << "  Balanced Brackets: "
            << (val.has_balanced_brackets ? "✓" : "⚠") << "\n";
        oss << "  Redundancy:       " << val.estimated_redundancy
            << (val.estimated_redundancy > 0.8 ? " (highly repetitive)" : "") << "\n";

        if (!val.warnings.empty()) {
            oss << "  Warnings:\n";
            for (auto& w : val.warnings) oss << "    - " << w << "\n";
        }

        oss << "  Symbol Distribution:\n";
        for (auto& [sym, count] : gen.stats.symbol_counts) {
            if (count > 0) oss << "    " << sym << ": " << count << "\n";
        }

        return oss.str();
    }

private:
    static void computeStats(GenerationResult& result) {
        auto& s = result.stats;
        s.length = result.string.size();
        s.branches = std::count(result.string.begin(), result.string.end(), '[');

        for (char c : result.string) {
            s.symbol_counts[c]++;
        }

        // Growth rate
        if (result.history.size() >= 2) {
            size_t first = result.history[0].size();
            size_t last = result.string.size();
            s.growth_rate = (first > 0)
                ? std::pow(static_cast<double>(last) / first,
                           1.0 / (result.history.size() - 1))
                : 0;
        }
    }

    static double estimateRedundancy(const std::string& s) {
        if (s.size() < 4) return 0;
        // Count repeating 2-char patterns
        std::map<std::string, size_t> patterns;
        for (size_t i = 0; i + 2 <= s.size(); ++i) {
            patterns[s.substr(i, 2)]++;
        }
        size_t unique = patterns.size();
        size_t max_possible = std::min(s.size() - 1, static_cast<size_t>(64));
        return 1.0 - static_cast<double>(unique) / max_possible;
    }

    static std::string trim(const std::string& s) {
        size_t start = 0, end = s.size();
        while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
        return s.substr(start, end - start);
    }
};

} // namespace nexus::utility::procgen
