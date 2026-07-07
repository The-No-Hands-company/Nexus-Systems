#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <map>
#include <set>
#include <functional>
#include <optional>

namespace nexus::utility::langtools {

/// Token stream debugger for lexer validation
class TokenStreamDebugger {
public:
    enum class TokenKind { Identifier, Number, String, Keyword, Operator, Punctuation, Comment, Whitespace, Eof, Unknown };

    struct Token { TokenKind kind; std::string lexeme; size_t line, col; };

    struct TokenStats {
        size_t total_tokens = 0;
        std::map<TokenKind, size_t> kind_counts;
        double avg_token_length = 0;
        double lexer_efficiency = 0;   // tokens per character
        std::vector<std::string> longest_tokens;
    };

    static TokenStats analyze(const std::vector<Token>& tokens) {
        TokenStats ts;
        ts.total_tokens = tokens.size();
        double total_len = 0;
        for (auto& t : tokens) {
            ts.kind_counts[t.kind]++;
            total_len += t.lexeme.size();
        }
        ts.avg_token_length = ts.total_tokens > 0 ? total_len / ts.total_tokens : 0;
        ts.lexer_efficiency = total_len > 0 ? static_cast<double>(ts.total_tokens) / total_len : 0;
        return ts;
    }

    static std::string report(const TokenStats& ts) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Token Stream Analysis ═══\n";
        oss << "  Tokens:          " << ts.total_tokens << "\n";
        oss << "  Avg Length:      " << ts.avg_token_length << "\n";
        for (auto& [k, c] : ts.kind_counts)
            oss << "  " << kindStr(k) << ": " << c << "\n";
        return oss.str();
    }

private:
    static std::string kindStr(TokenKind k) {
        switch(k) { case TokenKind::Identifier:return"Ident"; case TokenKind::Number:return"Number";
            case TokenKind::String:return"String"; case TokenKind::Keyword:return"Keyword";
            case TokenKind::Operator:return"Op"; case TokenKind::Punctuation:return"Punct";
            case TokenKind::Comment:return"Comment"; case TokenKind::Whitespace:return"WS";
            case TokenKind::Eof:return"EOF"; default:return"Unknown"; }
    }
};

/// Grammar fuzzer for parser stress testing
class GrammarFuzzer {
public:
    struct Production { std::string lhs; std::vector<std::string> rhs; };

    struct Grammar { std::string start; std::vector<Production> rules; };

    static std::string generate(const Grammar& g, int max_depth = 10, uint64_t seed = 42) {
        std::mt19937_64 rng(seed);
        return expand(g, g.start, max_depth, rng);
    }

private:
    static std::string expand(const Grammar& g, const std::string& symbol, int depth, std::mt19937_64& rng) {
        if (depth <= 0) return symbol;
        std::vector<const Production*> matches;
        for (auto& p : g.rules) if (p.lhs == symbol) matches.push_back(&p);
        if (matches.empty()) return symbol;

        std::uniform_int_distribution<size_t> dist(0, matches.size()-1);
        auto* chosen = matches[dist(rng)];

        std::string result;
        for (auto& s : chosen->rhs) {
            std::string expanded = expand(g, s, depth-1, rng);
            if (!result.empty() && !expanded.empty() &&
                !result.ends_with('(') && !expanded.starts_with(')'))
                result += ' ';
            result += expanded;
        }
        return result;
    }
};

/// Type checker debugger
class TypeCheckerDebugger {
public:
    struct TypeError { size_t line, col; std::string message; std::string expected, actual; };

    struct TypeContext {
        std::map<std::string, std::string> variables; // name -> type
        std::map<std::string, std::string> functions; // name -> return_type
    };

    struct TypeAnalysis {
        size_t error_count = 0;
        std::vector<TypeError> errors;
        size_t type_inference_count = 0;
        double type_annotation_coverage = 0;
    };

    static TypeAnalysis analyze(const std::vector<TypeError>& errors,
                                 size_t total_exprs, size_t annotated_exprs) {
        TypeAnalysis ta;
        ta.error_count = errors.size();
        ta.errors = errors;
        ta.type_annotation_coverage = total_exprs > 0 ? static_cast<double>(annotated_exprs)/total_exprs : 0;
        ta.type_inference_count = total_exprs - annotated_exprs;
        return ta;
    }

    static std::string report(const TypeAnalysis& ta) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Type Checker Debug ═══\n";
        oss << "  Errors:       " << ta.error_count << (ta.error_count>0?" ⚠":" ✓") << "\n";
        oss << "  Annot Coverage: " << (ta.type_annotation_coverage*100) << "%\n";
        oss << "  Inferences:   " << ta.type_inference_count << "\n";
        for (auto& e : ta.errors)
            oss << "  L" << e.line << ":" << e.col << " " << e.message << " (expected " << e.expected << ", got " << e.actual << ")\n";
        return oss.str();
    }
};

/// Optimization pass validator
class OptimizationValidator {
public:
    struct OptResult {
        bool preserves_semantics = true;
        bool improves_performance = false;
        double size_before = 0, size_after = 0;
        double estimated_speedup = 1.0;
        std::vector<std::string> regressions;
    };

    static OptResult validate(size_t ir_size_before, size_t ir_size_after,
                                double estimated_cycles_before, double estimated_cycles_after) {
        OptResult or_;
        or_.size_before = ir_size_before; or_.size_after = ir_size_after;
        or_.improves_performance = ir_size_after <= ir_size_before;
        or_.estimated_speedup = estimated_cycles_after > 0 ? estimated_cycles_before / estimated_cycles_after : 1;
        if (or_.estimated_speedup < 1.0) or_.regressions.push_back("Performance regression: " + std::to_string(or_.estimated_speedup) + "x");
        if (ir_size_after > ir_size_before * 1.1) or_.regressions.push_back("Code size bloat");
        return or_;
    }

    static std::string report(const OptResult& or_) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Optimization Validation ═══\n";
        oss << "  IR Size:     " << or_.size_before << " → " << or_.size_after
            << " (" << (or_.size_after>0?or_.size_before/or_.size_after:1) << "x)\n";
        oss << "  Speedup:     " << or_.estimated_speedup << "x\n";
        oss << "  Valid:       " << (or_.preserves_semantics ? "✓" : "⚠") << "\n";
        for (auto& r : or_.regressions) oss << "  ⚠ " << r << "\n";
        return oss.str();
    }
};

/// Symbol table analyzer
class SymbolTableAnalyzer {
public:
    struct Symbol { std::string name, kind, type; size_t scope_depth; size_t references; bool is_exported; };

    struct SymbolAnalysis {
        size_t total_symbols = 0;
        size_t shadowed_symbols = 0;
        size_t unused_symbols = 0;
        size_t exported_symbols = 0;
        double avg_references = 0;
        std::vector<std::string> most_referenced;
    };

    static SymbolAnalysis analyze(const std::vector<Symbol>& symbols) {
        SymbolAnalysis sa;
        sa.total_symbols = symbols.size();
        double ref_sum = 0; size_t max_ref = 0;
        std::map<std::string, std::vector<const Symbol*>> by_name;

        for (auto& s : symbols) {
            by_name[s.name].push_back(&s);
            ref_sum += s.references;
            if (s.references > max_ref) {
                max_ref = s.references;
                sa.most_referenced.clear();
                sa.most_referenced.push_back(s.name);
            } else if (s.references == max_ref && max_ref > 0) {
                sa.most_referenced.push_back(s.name);
            }
            if (s.is_exported) sa.exported_symbols++;
            if (s.references == 0) sa.unused_symbols++;
        }

        for (auto& [name, syms] : by_name)
            if (syms.size() > 1) sa.shadowed_symbols += syms.size() - 1;

        sa.avg_references = sa.total_symbols > 0 ? ref_sum / sa.total_symbols : 0;
        return sa;
    }

    static std::string report(const SymbolAnalysis& sa) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Symbol Table Analysis ═══\n";
        oss << "  Total:      " << sa.total_symbols << "\n";
        oss << "  Shadowed:   " << sa.shadowed_symbols << (sa.shadowed_symbols>0?" ⚠":" ✓") << "\n";
        oss << "  Unused:     " << sa.unused_symbols << (sa.unused_symbols>0?" ⚠":" ✓") << "\n";
        oss << "  Exported:   " << sa.exported_symbols << "\n";
        oss << "  Avg Refs:   " << sa.avg_references << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::langtools
