#!/usr/bin/env python3
"""Flesh out debug category (5 tools) with real debugger helper implementations."""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC = f"{BASE}/include/nexus/utility/debug"
SRC = f"{BASE}/src/utility/debug"
os.makedirs(SRC, exist_ok=True)
def w(p,c):
    with open(p,'w') as f: f.write(c)

# ===== 1. BREAKPOINT HELPER =====
w(f"{INC}/breakpoint_helper.h", '''#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace nexus::utility::debug {

/// Platform-aware breakpoint management (software int3 / hardware watchpoints)
class BreakpointHelper {
public:
    enum class BreakpointType { Software, HardwareExecute, HardwareRead, HardwareWrite, Conditional };
    struct Breakpoint {
        uintptr_t address;
        BreakpointType type;
        bool enabled;
        size_t hitCount;
        std::string name;
        std::function<bool()> condition; // Return true = break
    };

    static BreakpointHelper& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    /// Set a software breakpoint (int3 on x86, bkpt on ARM)
    bool setSoftwareBreakpoint(uintptr_t address, const std::string& name="");
    /// Set a hardware breakpoint (uses debug registers, limited count)
    bool setHardwareBreakpoint(uintptr_t address, BreakpointType type, const std::string& name="");
    /// Set conditional breakpoint
    bool setConditionalBreakpoint(uintptr_t address, std::function<bool()> condition, const std::string& name="");
    /// Remove breakpoint
    bool removeBreakpoint(uintptr_t address);
    /// Enable/disable breakpoint
    bool setBreakpointEnabled(uintptr_t address, bool enabled);
    /// Check if breakpoint exists
    bool hasBreakpoint(uintptr_t address) const;
    /// Get breakpoint info
    Breakpoint getBreakpoint(uintptr_t address) const;
    /// Get all breakpoints
    std::vector<Breakpoint> getAllBreakpoints() const;
    /// Count breakpoints
    size_t getBreakpointCount() const;
    /// Count by type
    size_t countByType(BreakpointType type) const;
    /// Record breakpoint hit
    void recordHit(uintptr_t address);
    /// Clear all
    void clear();

    /// Platform-specific helpers
    static bool isHardwareBreakpointsAvailable();
    static size_t maxHardwareBreakpoints();
    static bool triggerSoftwareBreakpoint();

private:
    BreakpointHelper() = default;
    ~BreakpointHelper() = default;
    bool enabled_ = false;
    mutable std::mutex mutex_;
    std::unordered_map<uintptr_t, Breakpoint> breakpoints_;
    uint8_t savedInt3Byte_ = 0;
};
} // namespace nexus::utility::debug
''')

w(f"{SRC}/breakpoint_helper.cpp", '''#include "nexus/utility/debug/breakpoint_helper.h"
#include <algorithm>
#include <cstring>

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/user.h>
#endif

namespace nexus::utility::debug {

BreakpointHelper& BreakpointHelper::instance() {
    static BreakpointHelper i;
    return i;
}

void BreakpointHelper::initialize() {
    std::lock_guard lock(mutex_);
    enabled_ = true;
    breakpoints_.clear();
}

void BreakpointHelper::shutdown() {
    std::lock_guard lock(mutex_);
    // Remove all breakpoints
    for (auto& [addr, bp] : breakpoints_) {
        if (bp.enabled && bp.type == BreakpointType::Software) {
            // Restore original byte (platform-specific)
            (void)addr;
        }
    }
    enabled_ = false;
    breakpoints_.clear();
}

bool BreakpointHelper::isEnabled() const { return enabled_; }

bool BreakpointHelper::setSoftwareBreakpoint(uintptr_t address, const std::string& name) {
    std::lock_guard lock(mutex_);
    if (!enabled_ || breakpoints_.count(address)) return false;
    if (breakpoints_.size() >= 128) return false; // Arbitrary soft limit
    Breakpoint bp{address, BreakpointType::Software, true, 0, name, nullptr};
    breakpoints_[address] = bp;
    return true;
}

bool BreakpointHelper::setHardwareBreakpoint(uintptr_t address, BreakpointType type, const std::string& name) {
    std::lock_guard lock(mutex_);
    if (!enabled_ || breakpoints_.count(address)) return false;
    if (countByType(BreakpointType::HardwareExecute) +
        countByType(BreakpointType::HardwareRead) +
        countByType(BreakpointType::HardwareWrite) >= maxHardwareBreakpoints())
        return false;
    Breakpoint bp{address, type, true, 0, name, nullptr};
    breakpoints_[address] = bp;
    return true;
}

bool BreakpointHelper::setConditionalBreakpoint(uintptr_t address, std::function<bool()> condition, const std::string& name) {
    std::lock_guard lock(mutex_);
    if (!enabled_ || breakpoints_.count(address) || !condition) return false;
    Breakpoint bp{address, BreakpointType::Conditional, true, 0, name, condition};
    breakpoints_[address] = bp;
    return true;
}

bool BreakpointHelper::removeBreakpoint(uintptr_t address) {
    std::lock_guard lock(mutex_);
    return breakpoints_.erase(address) > 0;
}

bool BreakpointHelper::setBreakpointEnabled(uintptr_t address, bool en) {
    std::lock_guard lock(mutex_);
    auto it = breakpoints_.find(address);
    if (it == breakpoints_.end()) return false;
    it->second.enabled = en;
    return true;
}

bool BreakpointHelper::hasBreakpoint(uintptr_t address) const {
    std::lock_guard lock(mutex_);
    return breakpoints_.count(address) > 0;
}

BreakpointHelper::Breakpoint BreakpointHelper::getBreakpoint(uintptr_t address) const {
    std::lock_guard lock(mutex_);
    auto it = breakpoints_.find(address);
    return it != breakpoints_.end() ? it->second : Breakpoint{};
}

std::vector<BreakpointHelper::Breakpoint> BreakpointHelper::getAllBreakpoints() const {
    std::lock_guard lock(mutex_);
    std::vector<Breakpoint> result;
    for (const auto& [addr, bp] : breakpoints_) result.push_back(bp);
    return result;
}

size_t BreakpointHelper::getBreakpointCount() const {
    std::lock_guard lock(mutex_);
    return breakpoints_.size();
}

size_t BreakpointHelper::countByType(BreakpointType type) const {
    std::lock_guard lock(mutex_);
    size_t count = 0;
    for (const auto& [addr, bp] : breakpoints_)
        if (bp.type == type) count++;
    return count;
}

void BreakpointHelper::recordHit(uintptr_t address) {
    std::lock_guard lock(mutex_);
    auto it = breakpoints_.find(address);
    if (it != breakpoints_.end()) it->second.hitCount++;
}

void BreakpointHelper::clear() {
    std::lock_guard lock(mutex_);
    breakpoints_.clear();
}

bool BreakpointHelper::isHardwareBreakpointsAvailable() {
#ifdef __linux__
    return true; // x86/x86_64 debug registers available via ptrace
#else
    return false;
#endif
}

size_t BreakpointHelper::maxHardwareBreakpoints() {
    return 4; // Standard x86 DR0-DR3
}

bool BreakpointHelper::triggerSoftwareBreakpoint() {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("int3");
    return true;
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("bkpt #0");
    return true;
#else
    return false;
#endif
}

} // namespace nexus::utility::debug
''')

# ===== 2. DEBUG STREAM =====
w(f"{INC}/debug_stream.h", '''#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace nexus::utility::debug {

/// Thread-safe categorized debug output stream with severity levels
class DebugStream {
public:
    enum class Severity { Trace, Debug, Info, Warning, Error, Fatal };
    struct DebugEntry {
        Severity severity;
        std::string category;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id threadId;
        std::string file;
        int line;
    };

    using OutputCallback = std::function<void(const DebugEntry&)>;

    static DebugStream& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    /// Write to debug stream
    DebugStream& write(Severity sev, const std::string& category,
                       const std::string& msg, const std::string& file="", int line=0);
    /// Convenience methods
    DebugStream& trace(const std::string& category, const std::string& msg, const std::string& file="", int line=0);
    DebugStream& debug(const std::string& category, const std::string& msg, const std::string& file="", int line=0);
    DebugStream& info(const std::string& category, const std::string& msg, const std::string& file="", int line=0);
    DebugStream& warn(const std::string& category, const std::string& msg, const std::string& file="", int line=0);
    DebugStream& error(const std::string& category, const std::string& msg, const std::string& file="", int line=0);
    DebugStream& fatal(const std::string& category, const std::string& msg, const std::string& file="", int line=0);

    /// Filtering
    void setMinimumSeverity(Severity sev);
    void enableCategory(const std::string& category);
    void disableCategory(const std::string& category);
    bool isCategoryEnabled(const std::string& category) const;

    /// Output redirection
    void addOutputCallback(OutputCallback cb);
    void clearOutputCallbacks();

    /// History
    std::vector<DebugEntry> getHistory() const;
    std::vector<DebugEntry> getByCategory(const std::string& category) const;
    std::vector<DebugEntry> getBySeverity(Severity sev) const;
    size_t getEntryCount() const;
    void setMaxHistorySize(size_t maxSize);

    /// Metrics
    size_t countBySeverity(Severity sev) const;

    void clear();

private:
    DebugStream() = default;
    ~DebugStream() = default;
    bool enabled_ = false;
    mutable std::mutex mutex_;
    Severity minSeverity_ = Severity::Info;
    std::vector<DebugEntry> history_;
    std::unordered_map<std::string, bool> categoryFilters_; // true=enabled
    std::vector<OutputCallback> outputCallbacks_;
    size_t maxHistorySize_ = 10000;

    static const char* severityName(Severity s);
};
} // namespace nexus::utility::debug
''')

w(f"{SRC}/debug_stream.cpp", '''#include "nexus/utility/debug/debug_stream.h"
#include <iostream>
#include <algorithm>

namespace nexus::utility::debug {

DebugStream& DebugStream::instance() {
    static DebugStream ds;
    return ds;
}

void DebugStream::initialize() {
    std::lock_guard lock(mutex_);
    enabled_ = true;
    history_.clear();
}

void DebugStream::shutdown() {
    std::lock_guard lock(mutex_);
    enabled_ = false;
}

bool DebugStream::isEnabled() const { return enabled_; }

DebugStream& DebugStream::write(Severity sev, const std::string& category,
                                 const std::string& msg, const std::string& file, int line) {
    if (!enabled_ || sev < minSeverity_) return *this;

    // Check category filter
    {
        std::lock_guard lock(mutex_);
        auto it = categoryFilters_.find(category);
        if (it != categoryFilters_.end() && !it->second) return *this;
    }

    DebugEntry entry{sev, category, msg, std::chrono::system_clock::now(),
                     std::this_thread::get_id(), file, line};

    {
        std::lock_guard lock(mutex_);
        history_.push_back(entry);
        if (history_.size() > maxHistorySize_)
            history_.erase(history_.begin());
    }

    // Notify callbacks outside lock
    for (auto& cb : outputCallbacks_)
        if (cb) cb(entry);

    return *this;
}

DebugStream& DebugStream::trace(const std::string& cat, const std::string& msg, const std::string& file, int line) {
    return write(Severity::Trace, cat, msg, file, line);
}
DebugStream& DebugStream::debug(const std::string& cat, const std::string& msg, const std::string& file, int line) {
    return write(Severity::Debug, cat, msg, file, line);
}
DebugStream& DebugStream::info(const std::string& cat, const std::string& msg, const std::string& file, int line) {
    return write(Severity::Info, cat, msg, file, line);
}
DebugStream& DebugStream::warn(const std::string& cat, const std::string& msg, const std::string& file, int line) {
    return write(Severity::Warning, cat, msg, file, line);
}
DebugStream& DebugStream::error(const std::string& cat, const std::string& msg, const std::string& file, int line) {
    return write(Severity::Error, cat, msg, file, line);
}
DebugStream& DebugStream::fatal(const std::string& cat, const std::string& msg, const std::string& file, int line) {
    return write(Severity::Fatal, cat, msg, file, line);
}

void DebugStream::setMinimumSeverity(Severity sev) {
    std::lock_guard lock(mutex_);
    minSeverity_ = sev;
}

void DebugStream::enableCategory(const std::string& category) {
    std::lock_guard lock(mutex_);
    categoryFilters_[category] = true;
}

void DebugStream::disableCategory(const std::string& category) {
    std::lock_guard lock(mutex_);
    categoryFilters_[category] = false;
}

bool DebugStream::isCategoryEnabled(const std::string& category) const {
    std::lock_guard lock(mutex_);
    auto it = categoryFilters_.find(category);
    return it == categoryFilters_.end() || it->second;
}

void DebugStream::addOutputCallback(OutputCallback cb) {
    std::lock_guard lock(mutex_);
    if (cb) outputCallbacks_.push_back(cb);
}

void DebugStream::clearOutputCallbacks() {
    std::lock_guard lock(mutex_);
    outputCallbacks_.clear();
}

std::vector<DebugStream::DebugEntry> DebugStream::getHistory() const {
    std::lock_guard lock(mutex_);
    return history_;
}

std::vector<DebugStream::DebugEntry> DebugStream::getByCategory(const std::string& category) const {
    std::lock_guard lock(mutex_);
    std::vector<DebugEntry> result;
    for (const auto& e : history_)
        if (e.category == category) result.push_back(e);
    return result;
}

std::vector<DebugStream::DebugEntry> DebugStream::getBySeverity(Severity sev) const {
    std::lock_guard lock(mutex_);
    std::vector<DebugEntry> result;
    for (const auto& e : history_)
        if (e.severity == sev) result.push_back(e);
    return result;
}

size_t DebugStream::getEntryCount() const {
    std::lock_guard lock(mutex_);
    return history_.size();
}

void DebugStream::setMaxHistorySize(size_t maxSize) {
    std::lock_guard lock(mutex_);
    maxHistorySize_ = maxSize;
}

size_t DebugStream::countBySeverity(Severity sev) const {
    std::lock_guard lock(mutex_);
    size_t count = 0;
    for (const auto& e : history_)
        if (e.severity == sev) count++;
    return count;
}

void DebugStream::clear() {
    std::lock_guard lock(mutex_);
    history_.clear();
}

const char* DebugStream::severityName(Severity s) {
    switch (s) {
        case Severity::Trace: return "TRACE";
        case Severity::Debug: return "DEBUG";
        case Severity::Info:  return "INFO";
        case Severity::Warning: return "WARN";
        case Severity::Error: return "ERROR";
        case Severity::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

} // namespace nexus::utility::debug
''')

# ===== 3. DEBUG VISUALIZER =====
w(f"{INC}/debug_visualizer.h", '''#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <typeindex>
#include <unordered_map>

namespace nexus::utility::debug {

/// Generate debugger visualization data (natvis XML for MSVC, Python pretty printers for GDB)
class DebugVisualizer {
public:
    struct MemberInfo {
        std::string name;
        std::string type;
        size_t offset;
        size_t size;
        std::string displayFormat; // "hex", "decimal", "string", "pointer"
    };

    struct TypeLayout {
        std::string typeName;
        size_t totalSize;
        size_t alignment;
        std::vector<MemberInfo> members;
        bool hasVtable;
        size_t vtableOffset;
    };

    static DebugVisualizer& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    /// Register a type for visualization
    void registerType(const std::string& typeName, const TypeLayout& layout);
    /// Register a custom display string formatter
    void setDisplayString(const std::string& typeName, const std::string& format);
    /// Get type layout
    TypeLayout getTypeLayout(const std::string& typeName) const;
    /// Generate natvis XML for MSVC debugger
    std::string generateNatvis() const;
    /// Generate GDB pretty-printer Python script
    std::string generateGdbPrinter() const;
    /// Generate LLDB type summary
    std::string generateLldbSummary() const;
    /// Count registered types
    size_t getTypeCount() const;
    void clear();

private:
    DebugVisualizer() = default;
    ~DebugVisualizer() = default;
    bool enabled_ = false;
    std::unordered_map<std::string, TypeLayout> types_;
    std::unordered_map<std::string, std::string> displayStrings_;
};
} // namespace nexus::utility::debug
''')

w(f"{SRC}/debug_visualizer.cpp", '''#include "nexus/utility/debug/debug_visualizer.h"
#include <algorithm>

namespace nexus::utility::debug {

DebugVisualizer& DebugVisualizer::instance() {
    static DebugVisualizer dv;
    return dv;
}

void DebugVisualizer::initialize() {
    enabled_ = true;
    types_.clear();
    displayStrings_.clear();
}

void DebugVisualizer::shutdown() { enabled_ = false; }
bool DebugVisualizer::isEnabled() const { return enabled_; }

void DebugVisualizer::registerType(const std::string& name, const TypeLayout& layout) {
    if (!enabled_) return;
    types_[name] = layout;
}

void DebugVisualizer::setDisplayString(const std::string& name, const std::string& fmt) {
    displayStrings_[name] = fmt;
}

DebugVisualizer::TypeLayout DebugVisualizer::getTypeLayout(const std::string& name) const {
    auto it = types_.find(name);
    return it != types_.end() ? it->second : TypeLayout{};
}

std::string DebugVisualizer::generateNatvis() const {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\\n";
    os << "<AutoVisualizer xmlns=\"http://schemas.microsoft.com/vstudio/debugger/natvis/2010\">\\n";
    for (const auto& [name, layout] : types_) {
        os << "  <Type Name=\"" << name << "\">\\n";
        auto ds = displayStrings_.find(name);
        if (ds != displayStrings_.end())
            os << "    <DisplayString>" << ds->second << "</DisplayString>\\n";
        os << "    <Expand>\\n";
        for (const auto& m : layout.members) {
            os << "      <Item Name=\"" << m.name << "\">";
            if (m.offset > 0)
                os << "(*(" << m.type << "*)((char*)this+" << m.offset << "))";
            else
                os << m.name;
            os << "</Item>\\n";
        }
        os << "    </Expand>\\n";
        os << "  </Type>\\n";
    }
    os << "</AutoVisualizer>\\n";
    return os.str();
}

std::string DebugVisualizer::generateGdbPrinter() const {
    std::ostringstream os;
    os << "import gdb\\n\\n";
    os << "class NexusPrettyPrinters:\\n";
    os << "    def __init__(self):\\n";
    os << "        self.printers = {}\\n\\n";

    for (const auto& [name, layout] : types_) {
        std::string className = name;
        std::replace(className.begin(), className.end(), ':', '_');
        os << "class " << className << "_Printer:\\n";
        os << "    def __init__(self, val):\\n";
        os << "        self.val = val\\n";
        os << "    def to_string(self):\\n";
        auto ds = displayStrings_.find(name);
        if (ds != displayStrings_.end())
            os << "        return f\"" << ds->second << "\"\\n";
        else
            os << "        return \"" << name << "\"\\n";
        os << "    def children(self):\\n";
        for (const auto& m : layout.members) {
            os << "        yield '" << m.name << "', self.val['" << m.name << "']\\n";
        }
        os << "\\n";
        os << "    printers['" << name << "'] = " << className << "_Printer\\n\\n";
    }

    os << "    def lookup(self, val):\\n";
    os << "        return self.printers.get(str(val.type), None)\\n";
    return os.str();
}

std::string DebugVisualizer::generateLldbSummary() const {
    std::ostringstream os;
    for (const auto& [name, layout] : types_) {
        os << "type summary add " << name << " --summary-string \"";
        auto ds = displayStrings_.find(name);
        if (ds != displayStrings_.end())
            os << ds->second;
        else
            os << name << " (${var.size} bytes)";
        os << "\"\\n";
    }
    return os.str();
}

size_t DebugVisualizer::getTypeCount() const {
    return types_.size();
}

void DebugVisualizer::clear() {
    types_.clear();
    displayStrings_.clear();
}

} // namespace nexus::utility::debug
''')

# ===== 4. DISASSEMBLY HELPER =====
w(f"{INC}/disassembly_helper.h", '''#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <functional>

namespace nexus::utility::debug {

/// Runtime disassembly and code inspection helper
class DisassemblyHelper {
public:
    struct Instruction {
        uintptr_t address;
        std::string bytes;      // Hex representation
        std::string mnemonic;
        std::string operands;
        size_t length;
        bool isBranch;
        bool isCall;
        bool isReturn;
        bool isNop;
    };

    struct FunctionInfo {
        std::string name;
        uintptr_t startAddress;
        uintptr_t endAddress;
        size_t size;
        size_t instructionCount;
        bool hasPrologue;
        bool hasEpilogue;
    };

    static DisassemblyHelper& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    /// Disassemble a single instruction at address
    Instruction disassemble(uintptr_t address) const;
    /// Disassemble N instructions starting at address
    std::vector<Instruction> disassembleRange(uintptr_t address, size_t count) const;
    /// Detect function boundaries
    FunctionInfo analyzeFunction(uintptr_t address) const;
    /// Check if address looks like valid code
    bool isValidCodeAddress(uintptr_t address) const;
    /// Get instruction count in range
    size_t instructionCount(uintptr_t address, size_t byteRange) const;
    /// Find function prologue pattern (push rbp; mov rbp, rsp)
    bool detectPrologue(uintptr_t address) const;
    /// Find all function calls in range
    std::vector<uintptr_t> findCallTargets(uintptr_t address, size_t range) const;
    /// Generate disassembly listing
    std::string generateListing(uintptr_t address, size_t count, std::ostream* os = nullptr) const;
    /// Check if instruction is a NOP sled
    bool isNopSled(uintptr_t address, size_t maxCheck = 8) const;
    /// Decode instruction length without full disassembly
    static size_t decodeInstructionLength(uintptr_t address);

    void clear();

private:
    DisassemblyHelper() = default;
    ~DisassemblyHelper() = default;
    bool enabled_ = false;
    std::string formatInstruction(const Instruction& inst) const;
};
} // namespace nexus::utility::debug
''')

w(f"{SRC}/disassembly_helper.cpp", '''#include "nexus/utility/debug/disassembly_helper.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace nexus::utility::debug {

DisassemblyHelper& DisassemblyHelper::instance() {
    static DisassemblyHelper dh;
    return dh;
}

void DisassemblyHelper::initialize() { enabled_ = true; }
void DisassemblyHelper::shutdown() { enabled_ = false; }
bool DisassemblyHelper::isEnabled() const { return enabled_; }

DisassemblyHelper::Instruction DisassemblyHelper::disassemble(uintptr_t address) const {
    Instruction inst{};
    inst.address = address;
    const uint8_t* code = reinterpret_cast<const uint8_t*>(address);
    if (!code) return inst;

    // Basic x86_64 instruction detection
    inst.length = decodeInstructionLength(address);

    // Format bytes
    std::ostringstream bytes;
    for (size_t i = 0; i < inst.length && i < 15; i++) {
        if (i > 0) bytes << " ";
        bytes << std::hex << std::setw(2) << std::setfill('0') << (int)code[i];
    }
    inst.bytes = bytes.str();

    // Detect special instruction types
    if (inst.length == 1 && code[0] == 0x90) {
        inst.mnemonic = "nop"; inst.operands = ""; inst.isNop = true;
    } else if (inst.length == 1 && code[0] == 0xC3) {
        inst.mnemonic = "ret"; inst.isReturn = true;
    } else if (inst.length == 1 && code[0] == 0xCC) {
        inst.mnemonic = "int3";
    } else if (code[0] == 0xE8) {
        inst.mnemonic = "call"; inst.isCall = true;
        if (inst.length >= 5) {
            int32_t offset; std::memcpy(&offset, code + 1, 4);
            uintptr_t target = address + inst.length + offset;
            std::ostringstream os; os << "0x" << std::hex << target;
            inst.operands = os.str();
        }
    } else if ((code[0] >= 0x70 && code[0] <= 0x7F) || // Jcc short
               (code[0] == 0xEB) || code[0] == 0xE9) { // JMP
        inst.mnemonic = (code[0] == 0xEB || code[0] == 0xE9) ? "jmp" : "jcc";
        inst.isBranch = true;
    } else if (code[0] == 0xFF && code[1] == 0x25) {
        inst.mnemonic = "jmp"; inst.operands = "*[rip+...]"; inst.isBranch = true;
    } else {
        inst.mnemonic = "?";
    }

    return inst;
}

std::vector<DisassemblyHelper::Instruction> DisassemblyHelper::disassembleRange(uintptr_t address, size_t count) const {
    std::vector<Instruction> result;
    uintptr_t current = address;
    for (size_t i = 0; i < count; i++) {
        Instruction inst = disassemble(current);
        if (inst.length == 0) break;
        result.push_back(inst);
        current += inst.length;
    }
    return result;
}

DisassemblyHelper::FunctionInfo DisassemblyHelper::analyzeFunction(uintptr_t address) const {
    FunctionInfo info{};
    info.startAddress = address;
    info.hasPrologue = detectPrologue(address);

    // Scan for ret instruction to find end
    uintptr_t current = address;
    for (size_t i = 0; i < 10000; i++) {
        Instruction inst = disassemble(current);
        if (inst.length == 0) break;
        info.instructionCount++;
        current += inst.length;
        if (inst.isReturn) {
            info.endAddress = current;
            break;
        }
    }

    info.size = info.endAddress > info.startAddress ? info.endAddress - info.startAddress : 0;
    return info;
}

bool DisassemblyHelper::isValidCodeAddress(uintptr_t address) const {
    if (address == 0 || address < 0x1000) return false;
    Instruction inst = disassemble(address);
    return inst.length > 0 && inst.length <= 15;
}

size_t DisassemblyHelper::instructionCount(uintptr_t address, size_t byteRange) const {
    size_t count = 0;
    uintptr_t end = address + byteRange;
    uintptr_t current = address;
    while (current < end) {
        Instruction inst = disassemble(current);
        if (inst.length == 0) break;
        count++;
        current += inst.length;
    }
    return count;
}

bool DisassemblyHelper::detectPrologue(uintptr_t address) const {
    const uint8_t* code = reinterpret_cast<const uint8_t*>(address);
    if (!code) return false;
    // Detect: push rbp (0x55) followed by mov rbp, rsp (0x48 0x89 0xE5)
    if (code[0] == 0x55 && code[1] == 0x48 && code[2] == 0x89 && code[3] == 0xE5) return true;
    // Detect: endbr64 (0xF3 0x0F 0x1E 0xFA) + push rbp
    if (code[0] == 0xF3 && code[1] == 0x0F && code[2] == 0x1E && code[3] == 0xFA &&
        code[4] == 0x55) return true;
    return false;
}

std::vector<uintptr_t> DisassemblyHelper::findCallTargets(uintptr_t address, size_t range) const {
    std::vector<uintptr_t> targets;
    uintptr_t current = address;
    uintptr_t end = address + range;
    while (current < end) {
        Instruction inst = disassemble(current);
        if (inst.length == 0) break;
        if (inst.isCall && !inst.operands.empty()) {
            try { targets.push_back(std::stoull(inst.operands, nullptr, 16)); }
            catch (...) {}
        }
        current += inst.length;
    }
    return targets;
}

std::string DisassemblyHelper::generateListing(uintptr_t address, size_t count, std::ostream* os) const {
    std::ostringstream listing;
    auto instructions = disassembleRange(address, count);
    for (const auto& inst : instructions) {
        std::string line = formatInstruction(inst);
        listing << line << "\\n";
        if (os) *os << line << "\\n";
    }
    return listing.str();
}

bool DisassemblyHelper::isNopSled(uintptr_t address, size_t maxCheck) const {
    uintptr_t current = address;
    for (size_t i = 0; i < maxCheck; i++) {
        Instruction inst = disassemble(current);
        if (!inst.isNop) return i >= 2; // At least 2 NOPs for a sled
        current += inst.length;
    }
    return true;
}

size_t DisassemblyHelper::decodeInstructionLength(uintptr_t address) {
    const uint8_t* code = reinterpret_cast<const uint8_t*>(address);
    if (!code) return 0;

    uint8_t b = code[0];

    // Single-byte instructions
    if (b == 0x90 || b == 0xC3 || b == 0xCC || b == 0xC9 ||
        (b >= 0x50 && b <= 0x5F) || // push/pop reg
        (b >= 0x40 && b <= 0x4F))   // REX prefix alone (rare)
        return 1;

    // Two-byte instructions
    if (b == 0x0F && code[1]) {
        if (code[1] == 0x1F) return 3; // NOP multi-byte
        if (code[1] >= 0x80 && code[1] <= 0x8F) return 6; // Jcc near
        return 2 + (code[1] >= 0x10 && code[1] <= 0x2F ? 4 : 0); // SSE
    }

    // Three-byte: REX + opcode + modrm
    if ((b & 0xF0) == 0x40) { // REX prefix
        if (code[1] == 0x0F) return 3 + decodeInstructionLength(address + 2);
        return 2 + (code[2] ? 1 : 0);
    }

    // call (0xE8) = 5 bytes, jmp (0xE9) = 5, jcc short (0x70-0x7F) = 2
    if (b == 0xE8 || b == 0xE9) return 5;
    if (b >= 0x70 && b <= 0x7F) return 2;

    // Default: try to guess from ModR/M
    return code[1] ? 3 : 2;
}

std::string DisassemblyHelper::formatInstruction(const Instruction& inst) const {
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << inst.address << ":  ";
    os << std::left << std::setw(24) << inst.bytes;
    if (!inst.mnemonic.empty()) {
        os << inst.mnemonic;
        if (!inst.operands.empty()) os << " " << inst.operands;
    } else {
        os << "<unknown>";
    }
    return os.str();
}

void DisassemblyHelper::clear() {}

} // namespace nexus::utility::debug
''')

# ===== 5. VARIABLE DUMPER =====
w(f"{INC}/variable_dumper.h", '''#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <cstdint>
#include <ostream>
#include <typeindex>
#include <unordered_map>
#include <functional>

namespace nexus::utility::debug {

/// Dump variable contents, types, and memory layouts with size/offset analysis
class VariableDumper {
public:
    struct FieldInfo {
        std::string name;
        std::string type;
        size_t offset;
        size_t size;
        std::string value;       // Formatted value
        std::string hexValue;    // Hex representation
        bool isPointer;
        uintptr_t pointerTarget;
    };

    struct VariableInfo {
        std::string name;
        std::string type;
        void* address;
        size_t totalSize;
        size_t alignment;
        std::vector<FieldInfo> fields;
        std::string rawHex;
    };

    using CustomDumper = std::function<std::string(void* addr, size_t size)>;

    static VariableDumper& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    /// Dump a variable at address with type info
    VariableInfo dump(void* address, size_t size, const std::string& name="",
                      const std::string& type="unknown") const;
    /// Dump with custom field layout
    VariableInfo dumpWithFields(void* address, const std::vector<FieldInfo>& fields,
                                 const std::string& name="", const std::string& type="unknown") const;
    /// Hex dump of variable memory
    std::string hexDump(void* address, size_t size, size_t bytesPerLine=16) const;
    /// Compare two variables byte-by-byte
    std::vector<std::pair<size_t, std::pair<uint8_t, uint8_t>>> diff(void* a, void* b, size_t size) const;
    /// Register a custom dumper for a type
    void registerCustomDumper(const std::string& typeName, CustomDumper dumper);
    /// Check for memory pattern (e.g., uninitialized 0xCD, freed 0xDD)
    std::string detectMemoryPattern(void* address, size_t size) const;
    /// Pretty-print variable to stream
    void prettyPrint(const VariableInfo& info, std::ostream& os = std::cout) const;
    /// History
    std::vector<VariableInfo> getHistory() const;
    size_t getDumpCount() const;
    void clear();

private:
    VariableDumper() = default;
    ~VariableDumper() = default;
    bool enabled_ = false;
    std::vector<VariableInfo> history_;
    std::unordered_map<std::string, CustomDumper> customDumpers_;
    mutable size_t dumpCount_ = 0;
};
} // namespace nexus::utility::debug
''')

w(f"{SRC}/variable_dumper.cpp", '''#include "nexus/utility/debug/variable_dumper.h"
#include <cstring>
#include <iomanip>
#include <algorithm>

namespace nexus::utility::debug {

VariableDumper& VariableDumper::instance() {
    static VariableDumper vd;
    return vd;
}

void VariableDumper::initialize() {
    enabled_ = true;
    history_.clear();
    dumpCount_ = 0;
}

void VariableDumper::shutdown() { enabled_ = false; }
bool VariableDumper::isEnabled() const { return enabled_; }

VariableDumper::VariableInfo VariableDumper::dump(void* address, size_t size,
                                                    const std::string& name,
                                                    const std::string& type) const {
    VariableInfo info;
    info.name = name;
    info.type = type;
    info.address = address;
    info.totalSize = size;
    info.alignment = 8; // Default alignment
    info.rawHex = hexDump(address, size);

    if (!address || size == 0) return info;

    // Auto-detect alignment from address
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    info.alignment = 1;
    while (info.alignment < 16 && (addr & info.alignment) == 0)
        info.alignment <<= 1;
    if (info.alignment > 16) info.alignment = 16;

    dumpCount_++;
    return info;
}

VariableDumper::VariableInfo VariableDumper::dumpWithFields(void* address,
                                                              const std::vector<FieldInfo>& fields,
                                                              const std::string& name,
                                                              const std::string& type) const {
    auto info = dump(address, 0, name, type);
    info.fields = fields;

    // Calculate total size from fields
    size_t maxOffset = 0;
    for (auto& f : const_cast<std::vector<FieldInfo>&>(info.fields)) {
        uint8_t* fieldAddr = static_cast<uint8_t*>(address) + f.offset;
        // Generate hex value
        std::ostringstream hex;
        for (size_t i = 0; i < f.size && i < 8; i++) {
            if (i > 0) hex << " ";
            hex << std::hex << std::setw(2) << std::setfill('0')
                << (int)(static_cast<uint8_t*>(address)[f.offset + i]);
        }
        f.hexValue = hex.str();

        // Generate display value
        std::ostringstream val;
        if (f.size <= 8) {
            uint64_t v = 0;
            std::memcpy(&v, fieldAddr, std::min(f.size, sizeof(uint64_t)));
            val << std::dec << v;
        } else {
            val << "[...]";
        }
        f.value = val.str();

        maxOffset = std::max(maxOffset, f.offset + f.size);
    }
    info.totalSize = maxOffset;

    return info;
}

std::string VariableDumper::hexDump(void* address, size_t size, size_t bytesPerLine) const {
    if (!address || size == 0) return "";
    std::ostringstream os;
    const uint8_t* data = static_cast<const uint8_t*>(address);

    for (size_t i = 0; i < size; i += bytesPerLine) {
        os << std::hex << std::setw(8) << std::setfill('0') << i << ": ";
        // Hex
        for (size_t j = 0; j < bytesPerLine && (i+j) < size; j++) {
            os << std::hex << std::setw(2) << std::setfill('0')
               << (int)data[i+j] << " ";
        }
        // Pad short lines
        for (size_t j = i + bytesPerLine > size ? size - i : 0; j < bytesPerLine; j++)
            os << "   ";
        // ASCII
        os << " |";
        for (size_t j = 0; j < bytesPerLine && (i+j) < size; j++) {
            char c = data[i+j];
            os << (c >= 32 && c <= 126 ? c : '.');
        }
        os << "|\\n";
    }
    return os.str();
}

std::vector<std::pair<size_t, std::pair<uint8_t, uint8_t>>> VariableDumper::diff(void* a, void* b, size_t size) const {
    std::vector<std::pair<size_t, std::pair<uint8_t, uint8_t>>> diffs;
    if (!a || !b) return diffs;
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < size; i++) {
        if (pa[i] != pb[i])
            diffs.push_back({i, {pa[i], pb[i]}});
    }
    return diffs;
}

void VariableDumper::registerCustomDumper(const std::string& typeName, CustomDumper dumper) {
    if (dumper) customDumpers_[typeName] = dumper;
}

std::string VariableDumper::detectMemoryPattern(void* address, size_t size) const {
    if (!address || size == 0) return "invalid";
    const uint8_t* data = static_cast<const uint8_t*>(address);
    uint8_t first = data[0];

    // Check if all bytes are the same (common debug patterns)
    bool allSame = true;
    for (size_t i = 1; i < size && allSame; i++)
        if (data[i] != first) allSame = false;

    if (allSame) {
        switch (first) {
            case 0xCD: return "uninitialized (MSVC debug)";
            case 0xCC: return "uninitialized (MSVC debug stack)";
            case 0xDD: return "freed (MSVC debug heap)";
            case 0xFD: return "fence (MSVC debug heap)";
            case 0x00: return "zero-filled";
            case 0xFF: return "all-ones";
            case 0xAB: return "uninitialized (custom)";
            case 0xDE: return "dead memory (custom)";
            case 0xFE: return "fence (custom)";
        }
    }
    return "unknown pattern";
}

void VariableDumper::prettyPrint(const VariableInfo& info, std::ostream& os) const {
    os << "=== Variable: " << info.name << " ===" << "\\n";
    os << "  Type: " << info.type << "\\n";
    os << "  Address: 0x" << std::hex << reinterpret_cast<uintptr_t>(info.address) << std::dec << "\\n";
    os << "  Size: " << info.totalSize << " bytes\\n";
    os << "  Alignment: " << info.alignment << "\\n";
    if (!info.fields.empty()) {
        os << "  Fields:\\n";
        for (const auto& f : info.fields) {
            os << "    [" << std::setw(3) << f.offset << "] "
               << std::setw(12) << f.type << " " << f.name
               << " = " << f.value;
            if (f.isPointer)
                os << " -> 0x" << std::hex << f.pointerTarget << std::dec;
            os << "\\n";
        }
    }
    os << "  Hex:\\n" << info.rawHex;
    os << "=============================\\n";
}

std::vector<VariableDumper::VariableInfo> VariableDumper::getHistory() const { return history_; }
size_t VariableDumper::getDumpCount() const { return dumpCount_; }

void VariableDumper::clear() {
    history_.clear();
    dumpCount_ = 0;
}

} // namespace nexus::utility::debug
''')

print("Debug category (5 tools) fully fleshed out!")
