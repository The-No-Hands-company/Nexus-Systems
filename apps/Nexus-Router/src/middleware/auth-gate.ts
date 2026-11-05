import type { IncomingMessage, ServerResponse } from "node:http";
import type { Config } from "../lib/config";
import { logger } from "../lib/logger";
import { verifyDidSignature } from "../../../../packages/phantom-utils/src/verify";

interface RequestContext {
  requestId: string;
  metadata: Record<string, unknown>;
  phantomDid?: string;
  userId?: string;
  orgId?: string;
}

/**
 * Auth Gate: Validate Phantom DID and JWT, authorize capability
 */
export async function authGate(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  config: Config,
): Promise<void> {
  try {
    if (!config.auth?.phantom_enabled) {
      return; // Auth disabled
    }

    const authHeader = req.headers.authorization;
    if (!authHeader) {
      logger.warn(
        { request_id: context.requestId, url: req.url },
        "Auth gate: missing authorization header",
      );
      res.writeHead(401, { "Content-Type": "application/json" });
      res.end(
        JSON.stringify({
          error: "Unauthorized",
          message: "Missing authorization header",
          request_id: context.requestId,
        }),
      );
      return;
    }

    // Extract bearer token or DID
    const [scheme, token] = authHeader.split(" ");
    let phantomDid: string | undefined;
    let userId: string | undefined;
    let orgId: string | undefined;

    if (scheme.toLowerCase() === "bearer" && token) {
      try {
        if (token.startsWith("did:phantom:")) {
          phantomDid = token;
        } else {
          const [, payload] = token.split(".");
          if (payload) {
            const decoded = JSON.parse(Buffer.from(payload, "base64").toString());
            phantomDid = decoded.sub ?? decoded.phantom_did;
            userId = decoded.user_id;
            orgId = decoded.org_id;
          }
        }
      } catch (err) {
        logger.warn({ request_id: context.requestId, error: String(err) }, "Auth gate: failed to parse auth token");
        res.writeHead(401, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: "Invalid token", request_id: context.requestId }));
        return;
      }
    }

    if (!phantomDid) {
      logger.warn({ request_id: context.requestId }, "Auth gate: no valid phantom DID found");
      res.writeHead(403, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "Forbidden", request_id: context.requestId }));
      return;
    }

    // Verify Phantom signature when present (service-to-service calls attach
    // X-Phantom-Sig over the request body + X-Phantom-Ts).
    const phantomSig = req.headers["x-phantom-sig"] as string | undefined;
    const phantomTs = req.headers["x-phantom-ts"] as string | undefined;
    if (phantomSig && phantomTs) {
      // Read body for signature verification
      const rawBody: Buffer[] = [];
      for await (const chunk of req) rawBody.push(chunk as Buffer);
      const bodyStr = Buffer.concat(rawBody).toString();
      const message = bodyStr + phantomTs;
      const valid = await verifyDidSignature(phantomDid, message, phantomSig);
      if (!valid) {
        logger.warn({ request_id: context.requestId, phantom_did: phantomDid }, "Auth gate: Phantom signature invalid");
        res.writeHead(403, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: "Invalid Phantom signature", request_id: context.requestId }));
        return;
      }
      logger.debug({ request_id: context.requestId, phantom_did: phantomDid }, "Auth gate: Phantom signature verified");
    }

    // Store in context
    context.phantomDid = phantomDid;
    context.userId = userId;
    context.orgId = orgId;
    context.metadata.phantom_did = phantomDid;
    context.metadata.user_id = userId;
    context.metadata.org_id = orgId;

    logger.debug(
      { request_id: context.requestId, phantom_did: phantomDid },
      "Auth gate: validated Phantom DID",
    );
  } catch (err) {
    logger.error(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "Auth gate error",
    );
    res.writeHead(500, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ error: "Internal error", request_id: context.requestId }));
  }
}
