import { describe, expect, it, vi } from "vitest";
import { render, screen } from "@testing-library/react";
import PublicEventPage from "./PublicEventPage";
import type { CalendarApi } from "./calendar-api";

const api = (getPublicEvent: CalendarApi["getPublicEvent"]): CalendarApi => ({
  listEvents: vi.fn(), getEvent: vi.fn(), createEvent: vi.fn(), updateEvent: vi.fn(), deleteEvent: vi.fn(), listShares: vi.fn(), grantShare: vi.fn(), revokeShare: vi.fn(), createPublicLink: vi.fn(), revokePublicLink: vi.fn(), getPublicEvent,
});

describe("PublicEventPage", () => {
  it("shows only the event fields intended for a public link", async () => {
    render(<PublicEventPage token="token" api={api(vi.fn(async () => ({ title: "Open studio", startTime: "2026-08-18T09:00:00.000Z", endTime: "2026-08-18T10:00:00.000Z", allDay: false, location: "Gallery", description: "Drop in" })))} />);
    expect(await screen.findByText("Open studio")).toBeTruthy();
    expect(screen.getByText("Gallery")).toBeTruthy();
    expect(screen.queryByText("usr-owner")).toBeNull();
    expect(screen.queryByRole("navigation")).toBeNull();
    expect((document as Document & { referrerPolicy: string }).referrerPolicy).toBe("no-referrer");
  });

  it("uses a neutral not-found state for unknown or revoked tokens", async () => {
    render(<PublicEventPage token="gone" api={api(vi.fn().mockRejectedValue({ reason: "not_found" }))} />);
    expect(await screen.findByText("This event is unavailable.")).toBeTruthy();
    expect(screen.queryByText("not_found")).toBeNull();
  });
});
