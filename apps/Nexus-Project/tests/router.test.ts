import { describe, expect, it } from "bun:test";
import { Router } from "../src/router";

describe("Router", () => {
  const router = new Router<string>();
  router.add("GET", "/tasks/:id", (ctx, params) => new Response(`${ctx}:${params.id}`));
  router.add("PATCH", "/tasks/:id", () => new Response("patched"));
  router.add("GET", "/projects/:id/tasks", () => new Response("list"));

  it("matches a pattern and decodes its parameters", async () => {
    const match = router.match("GET", "/tasks/a%20b");
    expect(match && "handler" in match).toBe(true);
    if (!match || !("handler" in match)) return;
    expect(await match.handler("ctx", match.params).text()).toBe("ctx:a b");
  });

  it("reports the allowed methods when only the method is wrong", () => {
    expect(router.match("DELETE", "/tasks/1")).toEqual({ allowed: ["GET", "PATCH"] });
  });

  it("does not match a different shape", () => {
    expect(router.match("GET", "/tasks/1/extra")).toBeNull();
    expect(router.match("GET", "/projects/1")).toBeNull();
  });

  it("refuses a malformed percent-encoding instead of throwing", () => {
    expect(router.match("GET", "/tasks/%E0%A4%A")).toBeNull();
  });
});
