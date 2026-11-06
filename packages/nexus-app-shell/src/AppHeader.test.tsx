import { describe, expect, it } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import AppHeader from "./AppHeader";

describe("AppHeader", () => {
  it("renders an accessible banner with home, app name, and slots", () => {
    const html = renderToStaticMarkup(
      <AppHeader
        homeHref="/"
        appName="Calendar"
        userSlot={<a href="/account">Ada</a>}
        utilitySlot={<button type="button">Notifications</button>}
      />,
    );

    expect(html).toContain('role="banner"');
    expect(html).toContain('href="/"');
    expect(html).toContain('aria-label="Nexus home"');
    expect(html).toContain("Nexus");
    expect(html).toContain("Calendar");
    expect(html).toContain('href="/account"');
    expect(html).toContain("Notifications");
  });

  it("uses an ordinary anchor for home navigation across documents", () => {
    const html = renderToStaticMarkup(
      <AppHeader homeHref="https://nexus.example/" appName="Calendar" />,
    );

    expect(html).toContain('<a href="https://nexus.example/"');
    expect(html).not.toContain("data-discover");
  });

  it("drops the Nexus wordmark when framed by the shell, but keeps the app name and slots", () => {
    const html = renderToStaticMarkup(
      <AppHeader
        homeHref="/"
        appName="Calendar"
        embedded
        utilitySlot={<button type="button">Notifications</button>}
      />,
    );

    expect(html).toContain('role="banner"');
    expect(html).not.toContain('aria-label="Nexus home"');
    expect(html).toContain("Calendar");
    expect(html).toContain("Notifications");
  });
});
