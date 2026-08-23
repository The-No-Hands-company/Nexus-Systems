import type { Request } from "express";

export interface PaginationParams {
  limit: number;
  offset: number;
  page: number;
}

export interface PaginatedResponse<T> {
  data: T[];
  meta: {
    total: number;
    page: number;
    limit: number;
    totalPages: number;
    hasNextPage: boolean;
    hasPrevPage: boolean;
  };
}

const MAX_LIMIT = 100;
const DEFAULT_LIMIT = 20;

export function parsePagination(req: Request): PaginationParams {
  const rawLimit = parseInt(req.query.limit as string, 10);
  const rawPage = parseInt(req.query.page as string, 10);

  const limit = Number.isNaN(rawLimit) || rawLimit < 1
    ? DEFAULT_LIMIT
    : Math.min(rawLimit, MAX_LIMIT);

  const page = Number.isNaN(rawPage) || rawPage < 1 ? 1 : rawPage;
  const offset = (page - 1) * limit;

  return { limit, offset, page };
}

export function buildPaginatedResponse<T>(
  data: T[],
  total: number,
  params: PaginationParams,
): PaginatedResponse<T> {
  const totalPages = Math.ceil(total / params.limit);
  return {
    data,
    meta: {
      total,
      page: params.page,
      limit: params.limit,
      totalPages,
      hasNextPage: params.page < totalPages,
      hasPrevPage: params.page > 1,
    },
  };
}
