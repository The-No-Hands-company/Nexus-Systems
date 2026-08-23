import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Shell from "./Shell";

/**
 * Shell now renders <Link> for the wordmark, the account link and the report
 * link, so it needs a router in context. It still fetches nothing — the
 * property that matters for testability — but "renderable with no providers at
 * all" is no longer true, and pretending otherwise would just move the failure
 * into whoever renders it next.
 */
function renderInRouter(ui: React.ReactElement) {
  return render(<MemoryRouter>{ui}</MemoryRouter>);
}

describe("Shell", () => {
  it("renders the three regions as landmarks", () => {
    // Landmarks rather than test ids: they are what a screen reader uses, so
    // asserting on them tests the accessibility the doctrine requires.
    renderInRouter(<Shell sidebar={<nav />}>content</Shell>);
    expect(screen.getByRole("banner")).toBeTruthy();
    expect(screen.getByRole("main")).toBeTruthy();
  });

  it("puts children in the content region, not the header", () => {
    renderInRouter(<Shell sidebar={<div />}>the app</Shell>);
    expect(screen.getByRole("main").textContent).toContain("the app");
    expect(screen.getByRole("banner").textContent).not.toContain("the app");
  });

  it("renders whatever sidebar it is given, scoped to the sidebar region", () => {
    renderInRouter(<Shell sidebar={<div>launcher here</div>}>x</Shell>);
    const sidebar = screen.getByRole("complementary", { name: "Applications" });
    expect(sidebar.textContent).toContain("launcher here");
    expect(screen.getByRole("main").textContent).not.toContain("launcher here");
  });

  it("lets tall content scroll instead of clipping it", () => {
    // The content region was overflow-hidden, which is right for a framed app
    // but silently cut every shell-native page off at the fold — the Cloud
    // console ended mid-list and looked complete.
    renderInRouter(<Shell sidebar={null}><div>tall</div></Shell>);
    const main = screen.getByRole("main");
    expect(main.className).toContain("overflow-y-auto");
    expect(main.className).not.toContain("overflow-hidden");
  });

  it("shows the wordmark in the header", () => {
    renderInRouter(<Shell sidebar={<div />}>x</Shell>);
    expect(screen.getByRole("banner").textContent).toContain("Nexus");
  });

  it("shows who is signed in, linking to their account", () => {
    renderInRouter(
      <Shell sidebar={<div />} user={{ username: "ada", email: "ada@example.com" }}>x</Shell>,
    );
    const link = screen.getByRole("link", { name: /ada/i });
    expect(link.getAttribute("href")).toBe("/account");
    // The address is the tooltip, not the label: it is longer, less readable
    // and not what someone scans for.
    expect(link.getAttribute("title")).toBe("ada@example.com");
  });

  it("omits the identity entirely when nobody is signed in", () => {
    // The shell renders for signed-out visitors too. An empty avatar or a
    // "null" label would be worse than showing nothing.
    renderInRouter(<Shell sidebar={<div />}>x</Shell>);
    expect(screen.queryByRole("link", { name: /account/i })).toBeNull();
  });

  it("always offers a way home and a way to report a problem", () => {
    renderInRouter(<Shell sidebar={<div />}>x</Shell>);
    expect(screen.getByRole("link", { name: /nexus home/i }).getAttribute("href")).toBe("/");
    expect(screen.getByRole("link", { name: /report a problem/i }).getAttribute("href")).toBe("/report");
  });
});

describe("Shell operator link", () => {
  it("offers founders the Operator panel", () => {
    renderInRouter(
      <Shell sidebar={<div />} user={{ username: "boss", email: "b@x.dev", role: "founder" }}>x</Shell>,
    );
    const link = screen.getByRole("link", { name: "Operator" });
    expect(link.getAttribute("href")).toBe("/admin");
  });

  it("hides it from ordinary members", () => {
    renderInRouter(
      <Shell sidebar={<div />} user={{ username: "sam", email: "s@x.dev", role: "user" }}>x</Shell>,
    );
    expect(screen.queryByRole("link", { name: "Operator" })).toBeNull();
  });

  it("hides it from nobody (signed out)", () => {
    renderInRouter(<Shell sidebar={<div />}>x</Shell>);
    expect(screen.queryByRole("link", { name: "Operator" })).toBeNull();
  });
});
