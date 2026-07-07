#pragma once
#include <string>
#include <functional>

namespace nexus::utility::cpp26 {

class DebuggingIntrospector {
public:
    struct DebuggerState {
        bool isDebuggerPresent = false;
        bool breakpointSucceeded = false;
        bool isBeingDebugged = false;
    };

    bool isDebuggerActive() const { return debuggerActive_; }

    void setDebuggerActive(bool active) { debuggerActive_ = active; }

    DebuggerState checkState() const {
        DebuggerState s;
        s.isDebuggerPresent = debuggerActive_;
        s.isBeingDebugged = debuggerActive_;
        return s;
    }

    bool tryBreakpoint() const {
        if (!debuggerActive_) return false;
#if defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
        __builtin_debugtrap();
        return true;
#else
        return false;
#endif
    }

    void setBreakHandler(std::function<void()> handler) { handler_ = std::move(handler); }

private:
    bool debuggerActive_ = false;
    std::function<void()> handler_;
};

} // namespace nexus::utility::cpp26
