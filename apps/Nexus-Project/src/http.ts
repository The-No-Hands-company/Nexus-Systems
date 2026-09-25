/** The one error shape every route returns. Thrown anywhere, mapped by server.ts. */
export class HttpError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
    readonly extra: Record<string, unknown> = {},
  ) {
    super(message);
  }
}

const HEADERS = {
  "content-type": "application/json; charset=utf-8",
  "x-content-type-options": "nosniff",
  // Project data is private: no shared cache may keep it, and a URL carrying a
  // task id must not travel to a third party in a Referer.
  "cache-control": "no-store",
  "referrer-policy": "no-referrer",
} as const;

export function json(body: unknown, status = 200, headers: Record<string, string> = {}): Response {
  return new Response(JSON.stringify(body), { status, headers: { ...HEADERS, ...headers } });
}

export function errorResponse(error: HttpError): Response {
  return json({ error: error.code, message: error.message, ...error.extra }, error.status);
}

export const notFound = (what = "resource") => new HttpError(404, "not_found", `${what} not found`);
export const forbidden = () => new HttpError(403, "forbidden", "your role does not allow this");
export const badRequest = (message: string) => new HttpError(400, "invalid_request", message);
export const unprocessable = (code: string, message: string, extra: Record<string, unknown> = {}) =>
  new HttpError(422, code, message, extra);
export const conflict = (code: string, message: string, extra: Record<string, unknown> = {}) =>
  new HttpError(409, code, message, extra);

/** Parses a JSON body. An empty body is `{}`; malformed JSON is a 400, never a crash. */
export async function readJson(req: Request): Promise<unknown> {
  const text = await req.text();
  if (!text) return {};
  try {
    return JSON.parse(text);
  } catch {
    throw badRequest("body is not valid JSON");
  }
}
