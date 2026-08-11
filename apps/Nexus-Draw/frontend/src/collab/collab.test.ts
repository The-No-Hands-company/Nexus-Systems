import { describe, expect, it } from "vitest";
import type { WebsocketProvider } from "y-websocket";
import * as Y from "yjs";
import { type ElementData, makeElement } from "../stores/model";
import { CollabBinding, presenceCount } from "./collab";
import { writeElements, yToElements } from "./yElements";

const fakeProvider = (doc: Y.Doc, wsconnected = false) =>
  ({
    doc,
    wsconnected,
    wsUnsuccessful: false,
    awareness: undefined,
    on: () => {},
    destroy: () => {},
  }) as unknown as WebsocketProvider;

describe("collab binding", () => {
  it("propagates a remote update to the onChange callback", async () => {
    const docA = new Y.Doc();
    const docB = new Y.Doc();
    writeElements(docA, [makeElement("line", { x1: 0, y1: 0, x2: 1, y2: 1 })]);
    const got: ElementData[][] = [];
    const binding = new CollabBinding(fakeProvider(docB), (els) => got.push(els));
    Y.applyUpdate(docB, Y.encodeStateAsUpdate(docA));
    await new Promise((r) => setTimeout(r, 10));
    expect(got.length).toBeGreaterThan(0);
    expect(got[got.length - 1][0].elementType).toBe("line");
    binding.destroy();
  });

  it("setElements writes local state into the provider doc while connected", () => {
    const doc = new Y.Doc();
    const binding = new CollabBinding(fakeProvider(doc, true), () => {});
    binding.setElements([makeElement("rectangle", { x: 1, y: 2, width: 10, height: 10 })]);
    expect(yToElements(doc).map((e) => e.id)).toHaveLength(1);
    binding.destroy();
  });

  it("setElements does not echo its own write back through onChange", () => {
    const doc = new Y.Doc();
    const got: ElementData[][] = [];
    const binding = new CollabBinding(fakeProvider(doc, true), (els) => got.push(els));
    binding.setElements([makeElement("ellipse", { x: 0, y: 0, rx: 5, ry: 5 })]);
    awaitTick();
    expect(got).toHaveLength(0);
    binding.destroy();
  });

  it("setElements is a no-op while disconnected", () => {
    const doc = new Y.Doc();
    const binding = new CollabBinding(fakeProvider(doc, false), () => {});
    binding.setElements([makeElement("line", { x1: 0, y1: 0, x2: 1, y2: 1 })]);
    expect(yToElements(doc)).toHaveLength(0);
    binding.destroy();
  });

  it("presenceCount reads awareness state size", () => {
    const binding = new CollabBinding(fakeProvider(new Y.Doc()), () => {});
    expect(presenceCount(binding)).toBe(0);
    binding.destroy();
  });
});

function awaitTick(): Promise<void> {
  return new Promise((r) => setTimeout(r, 10));
}
