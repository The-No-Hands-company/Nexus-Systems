export interface ApiListResponse<T> {
  items: T[];
  nextCursor: string | null;
  note?: string;
}

export interface ApiEntityResponse<T> {
  ok: boolean;
  entity: T;
  note?: string;
}

export interface ApiSummary {
  total: number;
  planned: number;
  stubbed: number;
  inProgress: number;
  ready: number;
}

export function listResponse<T>(items: T[], note?: string): ApiListResponse<T> {
  return {
    items,
    nextCursor: null,
    note,
  };
}

export function entityResponse<T>(entity: T, note?: string): ApiEntityResponse<T> {
  return {
    ok: true,
    entity,
    note,
  };
}
