import { describe, expect, it, vi } from "vitest";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import CalendarApp from "./CalendarApp";
import type { CalendarApi, CalEvent } from "./calendar-api";
import type { CalendarRuntime } from "./runtime";

const runtime: CalendarRuntime = { basePath: "/", apiBase: "/api/v1/calendar", publicBase: "/", shellContext: false };
const today = new Date("2026-08-18T12:00:00Z");
const event: CalEvent = {
  id: "event-1", title: "Design review", startTime: "2026-08-18T09:00:00.000Z", endTime: "2026-08-20T10:00:00.000Z",
  allDay: false, createdAt: "2026-08-01T00:00:00.000Z", ownerSubject: "usr-owner", access: "owner",
};

function api(overrides: Partial<CalendarApi> = {}): CalendarApi {
  const share = { eventId: "event-1", subject: "usr-team", permission: "editor" as const, createdBy: "usr-owner", createdAt: "2026-08-01T00:00:00.000Z", updatedAt: "2026-08-01T00:00:00.000Z" };
  return {
    listEvents: vi.fn<CalendarApi["listEvents"]>(async () => []), getEvent: vi.fn(), createEvent: vi.fn(async () => event), updateEvent: vi.fn(), deleteEvent: vi.fn(async () => undefined),
    listShares: vi.fn(async () => []), grantShare: vi.fn(async () => share), revokeShare: vi.fn(async () => undefined), createPublicLink: vi.fn(async () => ({ token: "abc", publicPath: "" })),
    revokePublicLink: vi.fn(async () => undefined), getPublicEvent: vi.fn(), ...overrides,
  };
}

describe("CalendarApp", () => {
  it("shows a loading state and a real empty state", async () => {
    let resolve!: (events: CalEvent[]) => void;
    const client = api({ listEvents: vi.fn<CalendarApi["listEvents"]>(() => new Promise<CalEvent[]>((done) => { resolve = done; })) });
    render(<CalendarApp runtime={runtime} api={client} today={today} />);
    expect(screen.getByText("Loading calendar…")).toBeTruthy();
    resolve([]);
    await waitFor(() => expect(screen.getByText("No events this month.")).toBeTruthy());
  });

  it("keeps offline failures distinct and retries", async () => {
    const client = api({ listEvents: vi.fn().mockRejectedValueOnce({ reason: "offline" }).mockResolvedValueOnce([]) });
    render(<CalendarApp runtime={runtime} api={client} today={today} />);
    await screen.findByText("Calendar is offline.");
    fireEvent.click(screen.getByRole("button", { name: "Retry" }));
    await waitFor(() => expect(client.listEvents).toHaveBeenCalledTimes(2));
  });

  it("selects today from the plus button and preserves values after a failed save", async () => {
    const client = api({ createEvent: vi.fn().mockRejectedValue({ reason: "validation" }) });
    render(<CalendarApp runtime={runtime} api={client} today={today} />);
    await screen.findByText("No events this month.");
    fireEvent.click(screen.getByRole("button", { name: "Create event for today" }));
    expect(screen.getByText("Tuesday, August 18")).toBeTruthy();
    fireEvent.change(screen.getByLabelText("Title"), { target: { value: "Keep this title" } });
    fireEvent.click(screen.getByRole("button", { name: "Save event" }));
    await screen.findByText("Check the event details and try again.");
    expect((screen.getByLabelText("Title") as HTMLInputElement).value).toBe("Keep this title");
  });

  it("renders one multi-day event on every overlapping day", async () => {
    render(<CalendarApp runtime={runtime} api={api({ listEvents: vi.fn(async () => [event]) })} today={today} />);
    await screen.findAllByText("Design review");
    expect(screen.getAllByText("Design review")).toHaveLength(3);
  });

  it("shows owner sharing controls but hides mutations from viewers", async () => {
    const owner = api({ listEvents: vi.fn(async () => [event]) });
    const mounted = render(<CalendarApp runtime={runtime} api={owner} today={today} />);
    await screen.findAllByText("Design review");
    fireEvent.click(screen.getAllByText("Design review")[0]!);
    expect(screen.getByRole("button", { name: "Manage sharing" })).toBeTruthy();
    expect(screen.getByRole("button", { name: "Delete event" })).toBeTruthy();
    mounted.unmount();

    render(<CalendarApp runtime={runtime} api={api({ listEvents: vi.fn<CalendarApi["listEvents"]>(async () => [{ ...event, access: "viewer" }]) })} today={today} />);
    await screen.findAllByText("Design review");
    fireEvent.click(screen.getAllByText("Design review")[0]!);
    expect(screen.queryByRole("button", { name: "Manage sharing" })).toBeNull();
    expect(screen.queryByRole("button", { name: "Delete event" })).toBeNull();
  });

  it("keeps a failed delete recoverable", async () => {
    const client = api({ listEvents: vi.fn(async () => [event]), deleteEvent: vi.fn().mockRejectedValue({ reason: "unavailable" }) });
    render(<CalendarApp runtime={runtime} api={client} today={today} />);
    await screen.findAllByText("Design review");
    fireEvent.click(screen.getAllByText("Design review")[0]!);
    fireEvent.click(screen.getByRole("button", { name: "Delete event" }));
    expect(await screen.findByText("Could not delete this event. Try again.")).toBeTruthy();
    expect(screen.getByRole("button", { name: "Retry" })).toBeTruthy();
  });

  it("creates, copies, grants, and revokes an owner's public share", async () => {
    const client = api({ listEvents: vi.fn(async () => [event]), createPublicLink: vi.fn(async () => ({ token: "token-1", publicPath: "/api/v1/calendar/public/token-1" })) });
    render(<CalendarApp runtime={runtime} api={client} today={today} />);
    await screen.findAllByText("Design review");
    fireEvent.click(screen.getAllByText("Design review")[0]!);
    fireEvent.click(screen.getByRole("button", { name: "Manage sharing" }));
    fireEvent.change(screen.getByLabelText("Person to share with"), { target: { value: "usr-team" } });
    fireEvent.click(screen.getByRole("button", { name: "Grant editor access" }));
    await waitFor(() => expect(client.grantShare).toHaveBeenCalledWith("event-1", "usr-team", "editor"));
    fireEvent.click(screen.getByRole("button", { name: "Create public link" }));
    await screen.findByRole("button", { name: "Copy public link" });
    fireEvent.click(screen.getByRole("button", { name: "Revoke public link" }));
    await waitFor(() => expect(client.revokePublicLink).toHaveBeenCalledWith("event-1"));
  });
});
