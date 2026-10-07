/** Block banned client tags (see lib/tagBan.ts). Synchronous: no DB, no cache. */
import type { Request, Response, NextFunction } from "express";
import { clientTag } from "../lib/clientTag";
import { isTagBanned } from "../lib/tagBan";

export function apiBanMiddleware(req: Request, res: Response, next: NextFunction): void {
  if (isTagBanned(clientTag(req))) {
    res.status(403).json({ error: "Access denied.", code: "CLIENT_BANNED" });
    return;
  }
  next();
}

export function siteBanMiddleware(req: Request, res: Response, next: NextFunction): void {
  if (isTagBanned(clientTag(req))) {
    res.status(403).send("Access denied.");
    return;
  }
  next();
}
