import { describe, expect, test } from "bun:test";
import {
  buildKnowledgeResponse,
  searchKnowledge,
  type KnowledgeCatalogApp,
} from "../src/ecosystem-knowledge";

const catalog: KnowledgeCatalogApp[] = [
  {
    id: "nexus-chat",
    name: "Nexus Chat",
    description: "Real-time messaging for Nexus users",
    path: "/chat",
    primaryShelf: "work",
    secondaryShelves: ["everyday"],
    audiences: ["general"],
    capabilities: ["chat", "messaging"],
    maturity: "stable",
    visibility: "default",
    reviewStatus: "approved",
    health: "healthy",
  },
];

describe("ecosystem knowledge contract", () => {
  test("builds provenance-bearing platform and catalog documents", () => {
    const response = buildKnowledgeResponse(catalog, {
      generatedAt: "2026-10-18T00:00:00.000Z",
      status: "ready",
      cloudUrl: "https://cloud.tnhc.dev",
    });

    expect(response.protocol).toBe("nexus-knowledge/1");
    expect(response.status).toBe("ready");
    expect(response.documents).toEqual(expect.arrayContaining([
      expect.objectContaining({ id: "platform-auth", kind: "platform", trust: "contract" }),
      expect.objectContaining({
        id: "app-nexus-chat",
        kind: "app",
        trust: "approved-catalog",
        content: expect.stringContaining("/chat"),
        source: expect.objectContaining({ ref: "nexus-cloud:/api/v1/catalog" }),
      }),
    ]));
  });

  test("search returns only matching documents and keeps provenance", () => {
    const response = buildKnowledgeResponse(catalog, {
      generatedAt: "2026-10-18T00:00:00.000Z",
      status: "ready",
      cloudUrl: "https://cloud.tnhc.dev",
    });
    const matches = searchKnowledge(response.documents, "authenticate login");
    expect(matches.map((document) => document.id)).toEqual(["platform-auth"]);
    expect(matches[0]?.source).toEqual(expect.objectContaining({ kind: "architecture" }));
  });

  test("an empty query returns the complete bounded index in stable order", () => {
    const response = buildKnowledgeResponse(catalog, {
      generatedAt: "2026-10-18T00:00:00.000Z",
      status: "degraded",
      cloudUrl: "https://cloud.tnhc.dev",
    });
    expect(searchKnowledge(response.documents, "").map((document) => document.id)).toEqual(
      response.documents.map((document) => document.id),
    );
    expect(response.source.status).toBe("degraded");
  });
});
