import { WebsocketProvider } from "y-websocket";
import * as Y from "yjs";
import type { ElementData } from "../stores/model";
import { elementsEqual, writeElements, yToElements } from "./yElements";

export class CollabBinding {
  private suppress = false;
  private onChange: (els: ElementData[]) => void;
  private observers: Array<() => void> = [];

  constructor(
    public provider: WebsocketProvider,
    onChange: (els: ElementData[]) => void,
  ) {
    this.onChange = onChange;
    const doc = provider.doc;
    const observe = () => {
      if (this.suppress) return;
      this.onChange(yToElements(doc));
    };
    const map = doc.getMap("elements");
    const order = doc.getArray("order");
    map.observe(observe);
    order.observe(observe);
    this.observers = [() => map.unobserve(observe), () => order.unobserve(observe)];
  }

  setElements(els: ElementData[]): void {
    if (!this.provider.wsconnected) return;
    const doc = this.provider.doc;
    // Only rewrite when the Y doc actually differs from the new local state —
    // avoids feeding identical updates back through the provider every store tick.
    if (elementsEqual(yToElements(doc), els)) return;
    this.suppress = true;
    try {
      writeElements(doc, els);
    } finally {
      this.suppress = false;
    }
  }

  destroy(): void {
    for (const unobserve of this.observers) unobserve();
    this.provider.destroy();
  }
}

export function connectCollab(
  boardId: string,
  baseUrl?: string,
  onChange?: (els: ElementData[]) => void,
): Promise<CollabBinding> {
  const wsProto = typeof location !== "undefined" && location.protocol === "https:" ? "wss:" : "ws:";
  const url =
    baseUrl ??
    (typeof location !== "undefined"
      ? `${wsProto}//${location.host}/api/v1/draw/ws`
      : "ws://127.0.0.1:3075/api/v1/draw/ws");
  // Constructing the provider (and thus the underlying WebSocket) can throw —
  // e.g. mixed-content: an insecure `ws://` socket on an HTTPS page. Never let
  // that take down the editor; resolve with an inert binding instead.
  let provider: WebsocketProvider;
  try {
    provider = new WebsocketProvider(url, boardId, new Y.Doc());
  } catch {
    provider = new WebsocketProvider(url, boardId, new Y.Doc(), {
      WebSocketPolyfill: class Noop {
        url = url;
        readyState = 3;
        send() {}
        close() {}
        addEventListener() {}
        removeEventListener() {}
      } as unknown as typeof WebSocket,
    });
    return Promise.resolve(new CollabBinding(provider, onChange ?? (() => {})));
  }
  return new Promise((resolve) => {
    const finish = () => resolve(new CollabBinding(provider, onChange ?? (() => {})));
    provider.on("sync", (isSynced: boolean) => {
      if (isSynced) finish();
    });
    setTimeout(finish, 3000); // safety in headless tests
  });
}

export function presenceCount(binding: CollabBinding): number {
  return binding.provider.awareness?.getStates()?.size ?? 0;
}
