import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CalendarView from "./CalendarView";
import * as api from "../../api";

beforeEach(() => {
  vi.restoreAllMocks();
});

function renderCalendar() {
  return render(
    <MemoryRouter>
      <CalendarView />
    </MemoryRouter>,
  );
}

describe("CalendarView", () => {
  it("renders month navigation buttons", () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    expect(screen.getByRole("button", { name: /previous month/i })).toBeTruthy();
    expect(screen.getByRole("button", { name: /next month/i })).toBeTruthy();
  });

  it("renders a 7-column weekday header grid", () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    for (const d of ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"]) {
      expect(screen.getByText(d)).toBeTruthy();
    }
  });

  it("shows 'Nothing scheduled' when there are no events for today", async () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    await waitFor(() => expect(screen.getByText("Nothing scheduled")).toBeTruthy());
  });

  it("renders events for the selected day", async () => {
    const today = new Date().toISOString().slice(0, 10);
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([
      { id: "ev1", title: "Team standup", startTime: `${today}T09:00:00Z` },
      { id: "ev2", title: "Lunch", startTime: `${today}T12:00:00Z` },
    ]);
    renderCalendar();
    await waitFor(() => expect(screen.getByText("Team standup")).toBeTruthy());
    expect(screen.getByText("Lunch")).toBeTruthy();
    expect(screen.getByText("09:00")).toBeTruthy();
    expect(screen.getByText("12:00")).toBeTruthy();
  });

  it("shows 'Calendar unavailable' on load error", async () => {
    vi.spyOn(api, "listCalendarEvents").mockRejectedValue(new Error("network"));
    renderCalendar();
    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByRole("alert").textContent).toMatch(/calendar unavailable/i);
  });

  it("opens the new event form when '+ New event' is clicked", async () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    await waitFor(() => expect(screen.getByText("Nothing scheduled")).toBeTruthy());
    fireEvent.click(screen.getByRole("button", { name: /\+ new event/i }));
    expect(screen.getByRole("heading", { name: "New event" })).toBeTruthy();
    expect(screen.getByLabelText("Title")).toBeTruthy();
  });

  it("cancels the form and returns to the events panel", async () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    await waitFor(() => expect(screen.getByText("Nothing scheduled")).toBeTruthy());
    fireEvent.click(screen.getByRole("button", { name: /\+ new event/i }));
    fireEvent.click(screen.getByRole("button", { name: "Cancel" }));
    await waitFor(() => expect(screen.getByText("Nothing scheduled")).toBeTruthy());
  });

  it("requires a title before submitting", async () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    await waitFor(() => expect(screen.getByText("Nothing scheduled")).toBeTruthy());
    fireEvent.click(screen.getByRole("button", { name: /\+ new event/i }));
    fireEvent.click(screen.getByRole("button", { name: "Save" }));
    expect(screen.getByRole("alert").textContent).toMatch(/title is required/i);
  });

  it("creates an event and shows it in the panel", async () => {
    const today = new Date().toISOString().slice(0, 10);
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    vi.spyOn(api, "createCalendarEvent").mockResolvedValue({
      id: "new-1",
      title: "New meeting",
      startTime: `${today}T10:00:00Z`,
    });
    renderCalendar();
    await waitFor(() => expect(screen.getByText("Nothing scheduled")).toBeTruthy());
    fireEvent.click(screen.getByRole("button", { name: /\+ new event/i }));
    fireEvent.change(screen.getByLabelText("Title"), { target: { value: "New meeting" } });
    fireEvent.click(screen.getByRole("button", { name: "Save" }));
    await waitFor(() => expect(screen.getByText("New meeting")).toBeTruthy());
  });

  it("navigates to the previous and next month", async () => {
    vi.spyOn(api, "listCalendarEvents").mockResolvedValue([]);
    renderCalendar();
    const heading = () => screen.getByRole("heading", { level: 1 });
    const initialMonth = heading().textContent ?? "";
    fireEvent.click(screen.getByRole("button", { name: /next month/i }));
    await waitFor(() => expect(heading().textContent).not.toBe(initialMonth));
    const nextMonth = heading().textContent ?? "";
    fireEvent.click(screen.getByRole("button", { name: /previous month/i }));
    await waitFor(() => expect(heading().textContent).toBe(initialMonth));
    expect(heading().textContent).not.toBe(nextMonth);
  });
});
