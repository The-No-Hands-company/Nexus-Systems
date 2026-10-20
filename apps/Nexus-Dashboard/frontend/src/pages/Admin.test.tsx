import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import Admin from "./Admin";

const ADMIN = { id: "u-admin", username: "boss", email: "boss@x.dev", role: "founder" };
const PLAIN = { id: "u1", username: "ada", email: "ada@x.dev", role: "user" };

const REQUESTS = [
  { id: "usr-1", username: "sam", email: "s@x.dev", status: "pending", note: "want to try it",
    createdAt: "2026-08-11T10:00:00Z" },
  { id: "usr-2", username: "tia", email: "t@x.dev", status: "pending",
    createdAt: "2026-08-11T11:00:00Z" },
];

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

let fetchSpy: ReturnType<typeof vi.fn> | null = null;

function stubFetch(who: typeof ADMIN | typeof PLAIN, overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL, init?: RequestInit) => {
    const u = String(url);
    const key = `${init?.method ?? "GET"} ${u}`;
    if (overrides[key]) return overrides[key]!();
    if (u === "https://api.tnhc.dev/api/v1/me") return jsonResponse({ user: who });
    if (u === "/ipa/v1/auth/access-requests") return jsonResponse({ requests: REQUESTS });
    if (u.endsWith("/approve") || u.endsWith("/reject")) return jsonResponse({ user: {} });
    if (u === "/ipa/v1/auth/invites") {
      return jsonResponse({ id: "inv-1", code: "c".repeat(32), expiresAt: "2026-09-10T00:00:00Z" }, 201);
    }
    // Cloud dev tools endpoints
    if (u === "/ipa/cloud/endpoints") return jsonResponse({ routes: [] });
    if (u === "/ipa/cloud/federation/peers") return jsonResponse({ peers: [] });
    if (u === "/ipa/cloud/federation/identity") return jsonResponse({ address: "ns:test", shortId: "T-1" });
    if (u === "/ipa/cloud/tools") return jsonResponse({ tools: [] });
    if (u === "/ipa/dev/services") return jsonResponse({ services: [
      { name: "auth", description: "Identity provider", healthy: true, latencyMs: 3 },
      { name: "cloud", description: "Control plane", healthy: true, latencyMs: 5 },
      { name: "draw", description: "Draw API", healthy: false, detail: "ECONNREFUSED" },
    ]});
    if (u === "/ipa/v1/auth/users") return jsonResponse({ users: [
      { id: "u1", username: "ada", email: "a@x.dev", role: "user", status: "active" },
      { id: "u2", username: "boss", email: "b@x.dev", role: "founder", status: "active" },
    ]});
    if (u === "/ipa/dev/tools") {
      return jsonResponse({
        root: "/repo/dhts",
        exists: true,
        tools: [
          { id: "graph", dir: "ecosystem-graph", name: "Ecosystem Graph",
            description: "Extracts the dependency graph.", command: "python3 extract_graph.py",
            runnable: { cmd: "python3", args: ["extract_graph.py"] } },
          { id: "vision-board", dir: "ecosystem-vision-board", name: "Vision Board",
            description: "Long-running board server.", command: "python3 server.py" },
        ],
      });
    }
    throw new Error(`unexpected fetch: ${key}`);
  });
  fetchSpy = spy;
  globalThis.fetch = spy as any;
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

afterEach(() => {
  if (fetchSpy) {
    delete (globalThis as any).fetch;
    fetchSpy = null;
  }
});

describe("Admin", () => {
  it("lists pending requests with the note the person wrote", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());
    expect(screen.getByText("tia")).toBeTruthy();
    expect(screen.getByText(/want to try it/)).toBeTruthy();
  });

  it("removes a request from the queue once approved", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());

    fireEvent.click(screen.getAllByRole("button", { name: /approve/i })[0]!);
    await waitFor(() => expect(screen.queryByText("sam")).toBeNull());
    expect(screen.getByText("tia")).toBeTruthy();
  });

  it("removes a request from the queue once rejected", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());

    fireEvent.click(screen.getAllByRole("button", { name: /reject/i })[0]!);
    await waitFor(() => expect(screen.queryByText("sam")).toBeNull());
  });

  it("keeps the request in the queue when approval fails", async () => {
    // Dropping it optimistically would tell the operator someone was approved
    // when they were not, and the person would never be able to claim.
    stubFetch(ADMIN, {
      "POST /ipa/v1/auth/access-requests/usr-1/approve": () =>
        jsonResponse({ error: "not_pending" }, 409),
    });
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());

    fireEvent.click(screen.getAllByRole("button", { name: /approve/i })[0]!);
    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText("sam")).toBeTruthy();
  });

  it("mints a link-only invite and shows the code once", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByRole("button", { name: /create link only/i })).toBeTruthy());

    fireEvent.click(screen.getByRole("button", { name: /create link only/i }));
    await waitFor(() => expect(screen.getByText("c".repeat(32))).toBeTruthy());
    expect(screen.getByText(/only time|save|shown once|cannot be shown again/i)).toBeTruthy();
    // The operator is handed a link to send, pointing at the page Auth serves;
    // a bare code left the invitee with nowhere to type it.
    expect(screen.getByText(`https://auth.tnhc.dev/join?code=${"c".repeat(32)}`)).toBeTruthy();
  });

  it("renders nothing for a non-admin", async () => {
    // Presentation only — the endpoints are guarded by users:approve
    // server-side, which is what actually enforces this.
    stubFetch(PLAIN);
    const { container } = render(<Admin />);
    await waitFor(() => expect(container.textContent).not.toContain("Loading"));
    expect(screen.queryByText("sam")).toBeNull();
    expect(screen.queryByRole("button", { name: /create link only/i })).toBeNull();
  });

  it("says so when the queue is empty rather than looking broken", async () => {
    stubFetch(ADMIN, {
      "GET /ipa/v1/auth/access-requests": () => jsonResponse({ requests: [] }),
    });
    render(<Admin />);
    await waitFor(() => expect(screen.getByText(/no pending|nothing waiting|queue is empty/i)).toBeTruthy());
  });
  it("lists dhts helpers with run buttons only where runnable", async () => {
    // Open by default: hiding the tools behind a toggle is why they read
    // as "missing" in practice.
    stubFetch(ADMIN);
    render(<Admin />);
    expect(await screen.findByText("Ecosystem Graph")).toBeTruthy();
    expect(screen.getByText("Vision Board")).toBeTruthy();
    // Runnable tool exposes Run; the long-running one must not.
    const runButtons = screen.getAllByRole("button", { name: /^run$/i });
    expect(runButtons.length).toBe(1);
  });

  it("shows service health with up and down indicators", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    expect(await screen.findByText("ECONNREFUSED")).toBeTruthy();
  });

  it("lists users with role dropdowns for founder", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("ada")).toBeTruthy());
    expect(screen.getByText("boss")).toBeTruthy();
    const selects = screen.getAllByRole("combobox");
    expect(selects.length).toBe(2);
  });
});

describe("Admin emailed invites", () => {
  const CODE = "d".repeat(32);
  const LINK = `https://auth.tnhc.dev/join?code=${CODE}`;
  const invited = (delivery: string) => () =>
    jsonResponse({ id: "inv-2", code: CODE, expiresAt: "2026-10-20T00:00:00Z", link: LINK, delivery }, 201);

  async function sendTo(email: string) {
    render(<Admin />);
    await waitFor(() => expect(screen.getByLabelText(/email/i)).toBeTruthy());
    fireEvent.change(screen.getByLabelText(/email/i), { target: { value: email } });
    fireEvent.click(screen.getByRole("button", { name: /send invite/i }));
  }

  it("sends the address to Auth and confirms the invite went out, keeping the link as a backup", async () => {
    stubFetch(ADMIN, { "POST /ipa/v1/auth/invites": invited("sent") });
    await sendTo("ada@example.com");
    await waitFor(() => expect(screen.getByText(/invite sent to ada@example.com/i)).toBeTruthy());
    expect(screen.getByText(LINK)).toBeTruthy();
    const call = fetchSpy!.mock.calls.find(([u, init]) => String(u) === "/ipa/v1/auth/invites" && init?.method === "POST");
    expect(JSON.parse(String(call![1]!.body))).toEqual({ email: "ada@example.com" });
  });

  it("tells the operator to send the link themselves when the email could not be sent", async () => {
    stubFetch(ADMIN, { "POST /ipa/v1/auth/invites": invited("failed") });
    await sendTo("ada@example.com");
    await waitFor(() => expect(screen.getByText(/could not be sent/i)).toBeTruthy());
    expect(screen.getByText(LINK)).toBeTruthy();
  });

  it("says when email is not set up on this node", async () => {
    stubFetch(ADMIN, { "POST /ipa/v1/auth/invites": invited("disabled") });
    await sendTo("ada@example.com");
    await waitFor(() => expect(screen.getByText(/email sending is not set up/i)).toBeTruthy());
  });

  it("keeps Send invite disabled until an address is typed", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByRole("button", { name: /send invite/i })).toBeTruthy());
    expect((screen.getByRole("button", { name: /send invite/i }) as HTMLButtonElement).disabled).toBe(true);
  });

  it("reports whether the approval email went out", async () => {
    stubFetch(ADMIN, {
      "POST /ipa/v1/auth/access-requests/usr-1/approve": () => jsonResponse({ user: {}, notice: "sent" }),
    });
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());
    fireEvent.click(screen.getAllByRole("button", { name: /approve/i })[0]!);
    await waitFor(() => expect(screen.getByText(/approved sam\. they were emailed/i)).toBeTruthy());
  });

  it("tells the operator to pass on the claim page when the approval email failed", async () => {
    stubFetch(ADMIN, {
      "POST /ipa/v1/auth/access-requests/usr-1/approve": () => jsonResponse({ user: {}, notice: "failed" }),
    });
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());
    fireEvent.click(screen.getAllByRole("button", { name: /approve/i })[0]!);
    await waitFor(() => expect(screen.getByText(/could not be emailed/i)).toBeTruthy());
    const notice = screen.getByText(/could not be emailed/i).closest("[role=status]") as HTMLElement;
    expect(within(notice).getByText("https://auth.tnhc.dev/claim")).toBeTruthy();
  });
});
