import assert from "node:assert/strict";
import { test } from "node:test";
import worker from "./src/index.ts";

const env = {
  NEXUS_INGRESS_URL: "https://email-ingress.tnhc.dev/internal/v1/cloudflare-email",
  NEXUS_INGRESS_TOKEN: "local-test-token",
  ALLOWED_RECIPIENTS: "info@tnhc.dev,zajfan@tnhc.dev",
};

test("forwards the original raw MIME body to the protected Nexus endpoint", async () => {
  const originalFetch = globalThis.fetch;
  const rawMessage = "From: sender@example.net\r\nSubject: hello\r\n\r\nbody\r\n";
  let captured;
  globalThis.fetch = async (url, init) => {
    captured = { url, init, raw: await new Response(init.body).text() };
    return new Response("{}", { status: 202 });
  };
  try {
    await worker.email({
      from: "sender@example.net",
      to: "info@tnhc.dev",
      raw: new ReadableStream({ start(controller) { controller.enqueue(new TextEncoder().encode(rawMessage)); controller.close(); } }),
      rawSize: new TextEncoder().encode(rawMessage).length,
      setReject(reason) { throw new Error(`unexpected rejection: ${reason}`); },
    }, env);
  } finally {
    globalThis.fetch = originalFetch;
  }
  assert.equal(captured.url, env.NEXUS_INGRESS_URL);
  assert.equal(captured.init.method, "POST");
  assert.equal(captured.init.headers.Authorization, `Bearer ${env.NEXUS_INGRESS_TOKEN}`);
  assert.equal(captured.init.headers["X-Nexus-Envelope-To"], "info@tnhc.dev");
  assert.equal(captured.raw, rawMessage);
});

test("rejects recipients outside the two configured mailboxes", async () => {
  let rejected;
  const originalFetch = globalThis.fetch;
  globalThis.fetch = async () => { throw new Error("must not call Nexus"); };
  try {
    await worker.email({
      from: "sender@example.net",
      to: "other@tnhc.dev",
      raw: new ReadableStream({ start(controller) { controller.close(); } }),
      rawSize: 0,
      setReject(reason) { rejected = reason; },
    }, env);
  } finally {
    globalThis.fetch = originalFetch;
  }
  assert.equal(rejected, "Recipient is not hosted here");
});
