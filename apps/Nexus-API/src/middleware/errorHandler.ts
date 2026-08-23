import type { Request, Response, NextFunction } from "express";
import { AppError } from "../lib/errors";
import logger from "../lib/logger";

export function globalErrorHandler(
  err: unknown,
  req: Request,
  res: Response,
  _next: NextFunction,
): void {
  const requestId = (req.headers["x-request-id"] as string) ?? "unknown";
  const isProd = process.env.NODE_ENV === "production";

  if (err instanceof AppError && err.isOperational) {
    logger.warn({ err, requestId, path: req.path, method: req.method }, err.message);
    res.status(err.statusCode).json({
      error: { message: err.message, code: err.code, requestId },
    });
    return;
  }

  logger.error(
    { err, requestId, path: req.path, method: req.method },
    "Unhandled error",
  );

  res.status(500).json({
    error: {
      message: isProd ? "An unexpected error occurred. Please try again." : String(err),
      code: "INTERNAL_ERROR",
      requestId,
      ...(isProd ? {} : { stack: err instanceof Error ? err.stack : undefined }),
    },
  });
}

export function notFoundHandler(req: Request, res: Response): void {
  res.status(404).json({
    error: {
      message: `Route ${req.method} ${req.path} not found`,
      code: "NOT_FOUND",
    },
  });
}
