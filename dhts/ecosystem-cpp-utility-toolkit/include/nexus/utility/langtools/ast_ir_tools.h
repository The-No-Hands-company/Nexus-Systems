#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <functional>

namespace nexus::utility::langtools {

/// AST node representation and differ for compiler debugging
class AstDiffer {
public:
    enum class NodeKind { Program, Function, Block, If, While, For, Return, Expr, Call, Var, Literal, Binary, Unary, Unknown };

    struct AstNode {
        NodeKind kind = NodeKind::Unknown;
        std::string value;
        std::string type;
        size_t line = 0, col = 0;
        std::vector<std::shared_ptr<AstNode>> children;
    };

    struct DiffEntry {
        enum Op { Added, Removed, Changed, Unchanged };
        Op operation;
        std::string path;
        std::string before, after;
    };

    struct DiffResult {
        std::vector<DiffEntry> entries;
        size_t added = 0, removed = 0, changed = 0;
        double similarity = 1.0; // 0=completely different, 1=identical
    };

    static DiffResult diff(const AstNode& before, const AstNode& after) {
        DiffResult r;
        diffNodes(before, after, "", r);
        size_t total = r.added + r.removed + r.changed;
        size_t nodes = countNodes(before) + countNodes(after);
        r.similarity = nodes > 0 ? 1.0 - static_cast<double>(total) / nodes : 1;
        return r;
    }

    static std::string report(const DiffResult& dr) {
        std::ostringstream oss;
        oss << "═══ AST Diff Report ═══\n";
        oss << "  Added:    " << dr.added << "\n";
        oss << "  Removed:  " << dr.removed << "\n";
        oss << "  Changed:  " << dr.changed << "\n";
        oss << "  Similarity: " << (dr.similarity * 100) << "%\n";
        for (auto& e : dr.entries) {
            if (e.operation != DiffEntry::Unchanged)
                oss << "  " << opStr(e.operation) << " " << e.path << "\n";
        }
        return oss.str();
    }

private:
    static void diffNodes(const AstNode& a, const AstNode& b, const std::string& path, DiffResult& r) {
        if (a.kind != b.kind || a.value != b.value) {
            DiffEntry e{DiffEntry::Changed, path, kindStr(a.kind)+":"+a.value, kindStr(b.kind)+":"+b.value};
            r.entries.push_back(e); r.changed++; return;
        }
        r.entries.push_back({DiffEntry::Unchanged, path, "", ""});

        size_t max_c = std::max(a.children.size(), b.children.size());
        for (size_t i = 0; i < max_c; ++i) {
            std::string child_path = path + "/" + std::to_string(i);
            if (i >= a.children.size()) {
                r.entries.push_back({DiffEntry::Added, child_path, "", "node"});
                r.added++;
            } else if (i >= b.children.size()) {
                r.entries.push_back({DiffEntry::Removed, child_path, "node", ""});
                r.removed++;
            } else {
                diffNodes(*a.children[i], *b.children[i], child_path, r);
            }
        }
    }

    static size_t countNodes(const AstNode& n) {
        size_t c = 1;
        for (auto& ch : n.children) c += countNodes(*ch);
        return c;
    }

    static std::string kindStr(NodeKind k) {
        switch (k) {
            case NodeKind::Program: return "Program"; case NodeKind::Function: return "Function";
            case NodeKind::Block: return "Block"; case NodeKind::If: return "If";
            case NodeKind::While: return "While"; case NodeKind::For: return "For";
            case NodeKind::Return: return "Return"; case NodeKind::Expr: return "Expr";
            case NodeKind::Call: return "Call"; case NodeKind::Var: return "Var";
            case NodeKind::Literal: return "Literal"; case NodeKind::Binary: return "Binary";
            case NodeKind::Unary: return "Unary"; default: return "Unknown";
        }
    }

    static std::string opStr(DiffEntry::Op o) { return o==DiffEntry::Added?"+":o==DiffEntry::Removed?"-":"~"; }
};

/// IR/bytecode validator for compiler middle-end debugging
class IrValidator {
public:
    enum class IrOp { Load, Store, Add, Sub, Mul, Div, Call, Ret, Branch, Cmp, Phi, Alloca, GEP, Cast, Unknown };

    struct Instruction {
        IrOp op;
        std::string result;
        std::vector<std::string> operands;
        size_t line = 0;
    };

    struct BasicBlock { std::string label; std::vector<Instruction> instructions; std::string terminator_label; };

    struct ValidationResult {
        bool valid = true;
        bool has_entry = false, has_exit = false;
        bool is_ssa = false;
        size_t unreachable_blocks = 0;
        size_t use_before_def = 0;
        std::vector<std::string> errors;
    };

    static ValidationResult validate(const std::vector<BasicBlock>& blocks) {
        ValidationResult vr;
        if (blocks.empty()) { vr.errors.push_back("No basic blocks"); vr.valid = false; return vr; }

        vr.has_entry = (blocks[0].label == "entry" || blocks[0].label == "start");
        for (auto& b : blocks) if (b.terminator_label == "exit" || b.label == "exit") vr.has_exit = true;

        std::map<std::string, bool> defined;
        for (auto& b : blocks) {
            for (auto& inst : b.instructions) {
                if (!inst.result.empty()) defined[inst.result] = true;
                for (auto& op : inst.operands) {
                    if (op[0] == '%' && !defined[op] && op != "%entry") {
                        vr.use_before_def++;
                        vr.errors.push_back("Use before def: " + op + " in block " + b.label);
                    }
                }
            }
        }

        vr.valid = vr.has_entry && vr.has_exit && vr.errors.empty();
        return vr;
    }

    static std::string report(const ValidationResult& vr) {
        std::ostringstream oss;
        oss << "═══ IR Validation ═══\n";
        oss << "  Valid:      " << (vr.valid ? "✓" : "⚠") << "\n";
        oss << "  Entry/Exit: " << (vr.has_entry?"✓":"✗") << "/" << (vr.has_exit?"✓":"✗") << "\n";
        oss << "  Use-before-def: " << vr.use_before_def << (vr.use_before_def>0?" ⚠":" ✓") << "\n";
        for (auto& e : vr.errors) oss << "  ⚠ " << e << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::langtools
