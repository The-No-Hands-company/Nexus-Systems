import React, { Component, type ReactNode, type ErrorInfo } from "react";
import { createRoot } from "react-dom/client";
import "./index.css";
import App from "./App";

class ErrorBoundary extends Component<{ children: ReactNode }, { error: Error | null }> {
  state = { error: null };
  static getDerivedStateFromError(error: Error) { return { error }; }
  componentDidCatch(_error: Error, info: ErrorInfo) {
    console.error("[nexus-project] render error", info.componentStack);
  }
  render() {
    const { error } = this.state;
    if (error) {
      return (
        <div style={{ padding: "2rem", color: "#e15d5d", fontFamily: "monospace", fontSize: "13px", background: "#030303", minHeight: "100vh" }}>
          <strong style={{ display: "block", marginBottom: "0.5rem" }}>Something went wrong</strong>
          <pre style={{ whiteSpace: "pre-wrap", wordBreak: "break-word" }}>{String(error)}</pre>
        </div>
      );
    }
    return this.props.children;
  }
}

const root = document.getElementById("root")!;
createRoot(root).render(
  <React.StrictMode>
    <ErrorBoundary>
      <App />
    </ErrorBoundary>
  </React.StrictMode>
);
