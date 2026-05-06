export interface HierarchyItemLike {
  name: string;
}

export interface HierarchyTreeItemLike extends HierarchyItemLike {
  id: number;
  parentId: number | null;
}

export interface HierarchyRow<T extends HierarchyTreeItemLike> {
  item: T;
  depth: number;
}

export function filterHierarchyItems<T extends HierarchyItemLike>(items: T[], query: string): T[] {
  const normalized = query.trim().toLowerCase();
  if (!normalized) return items;
  return items.filter((item) => item.name.toLowerCase().includes(normalized));
}

export function buildUniqueObjectName(baseName: string, existingNames: Iterable<string>): string {
  const normalizedBase = baseName.trim() || "Object";
  const used = new Set(existingNames);
  if (!used.has(normalizedBase)) {
    return normalizedBase;
  }

  let index = 2;
  while (used.has(`${normalizedBase}_${index}`)) {
    index += 1;
  }
  return `${normalizedBase}_${index}`;
}

export function collectDescendantIds<T extends HierarchyTreeItemLike>(
  items: T[],
  rootId: number
): Set<number> {
  const childrenByParent = new Map<number | null, T[]>();
  for (const item of items) {
    const siblings = childrenByParent.get(item.parentId) ?? [];
    siblings.push(item);
    childrenByParent.set(item.parentId, siblings);
  }

  const descendants = new Set<number>();
  const stack = [rootId];
  while (stack.length > 0) {
    const currentId = stack.pop()!;
    const children = childrenByParent.get(currentId) ?? [];
    for (const child of children) {
      if (descendants.has(child.id)) continue;
      descendants.add(child.id);
      stack.push(child.id);
    }
  }

  return descendants;
}

export function buildHierarchyRows<T extends HierarchyTreeItemLike>(
  items: T[],
  query = "",
  collapsedIds: ReadonlySet<number> = new Set<number>()
): HierarchyRow<T>[] {
  const normalized = query.trim().toLowerCase();
  const itemMap = new Map(items.map((item) => [item.id, item]));
  const childrenByParent = new Map<number | null, T[]>();

  for (const item of items) {
    const siblings = childrenByParent.get(item.parentId) ?? [];
    siblings.push(item);
    childrenByParent.set(item.parentId, siblings);
  }

  for (const siblings of childrenByParent.values()) {
    siblings.sort((a, b) => a.name.localeCompare(b.name));
  }

  const visibleIds = new Set<number>();
  if (!normalized) {
    for (const item of items) visibleIds.add(item.id);
  } else {
    for (const item of items) {
      if (!item.name.toLowerCase().includes(normalized)) continue;
      visibleIds.add(item.id);
      let cursor = item.parentId;
      while (cursor !== null) {
        visibleIds.add(cursor);
        cursor = itemMap.get(cursor)?.parentId ?? null;
      }
    }
  }

  const rows: HierarchyRow<T>[] = [];
  const visit = (parentId: number | null, depth: number) => {
    const children = childrenByParent.get(parentId) ?? [];
    for (const child of children) {
      if (!visibleIds.has(child.id)) continue;
      rows.push({ item: child, depth });
      if (!normalized && collapsedIds.has(child.id)) {
        continue;
      }
      visit(child.id, depth + 1);
    }
  };

  visit(null, 0);
  return rows;
}
