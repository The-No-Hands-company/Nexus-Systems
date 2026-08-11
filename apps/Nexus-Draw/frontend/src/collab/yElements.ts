import * as Y from "yjs";
import type { ElementData } from "../stores/model";

const ELEMENTS = "elements";
const ORDER = "order";

function toStored(el: ElementData): ElementData {
  return {
    id: el.id,
    elementType: el.elementType,
    data: { ...el.data },
    style: { ...el.style },
    transform: { ...el.transform },
    order: el.order,
    seed: el.seed,
  };
}

function getOurIds(doc: Y.Doc): Set<string> {
  if (!(doc as any)._ourIds) {
    (doc as any)._ourIds = new Set<string>();
  }
  return (doc as any)._ourIds;
}

export function createElementDoc(initial: ElementData[] = []): Y.Doc {
  const doc = new Y.Doc();
  writeElements(doc, initial);
  return doc;
}

export function writeElements(doc: Y.Doc, elements: ElementData[]): void {
  const map = doc.getMap<ElementData>(ELEMENTS);
  const order = doc.getArray<string>(ORDER);
  const ourIds = getOurIds(doc);
  const newIds = new Set(elements.map((e) => e.id));
  const prevOurIds = new Set(ourIds);

  const toDelete = [...ourIds].filter((id) => !newIds.has(id));
  const toAddOrUpdate = elements.filter((el) => !prevOurIds.has(el.id) || JSON.stringify(map.get(el.id)) !== JSON.stringify(toStored(el)));

  doc.transact(() => {
    for (const id of toDelete) {
      map.delete(id);
      ourIds.delete(id);
    }
    for (const el of toAddOrUpdate) {
      map.set(el.id, toStored(el));
      ourIds.add(el.id);
    }
    const current = order.toArray();
    const next = elements.map((e) => e.id);
    if (current.join("\u0000") !== next.join("\u0000")) {
      order.delete(0, order.length);
      order.insert(0, next);
    }
  });
}

export function yToElements(doc: Y.Doc): ElementData[] {
  const map = doc.getMap<ElementData>(ELEMENTS);
  const order = doc.getArray<string>(ORDER).toArray();
  const byId = new Map<string, ElementData>();
  for (const [id, el] of map) byId.set(id, toStored(el));
  const used = new Set<string>();
  const out: ElementData[] = [];
  for (const id of order) {
    const el = byId.get(id);
    if (el) {
      out.push(toStored(el));
      used.add(id);
    }
  }
  for (const [id, el] of byId) {
    if (!used.has(id)) out.push(toStored(el));
  }
  return out;
}

export function elementsEqual(a: ElementData[], b: ElementData[]): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}
