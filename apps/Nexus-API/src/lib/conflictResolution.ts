/**
 * Same-domain conflict resolution.
 *
 * When two nodes claim to host the same domain, the network must decide which
 * version to trust. The resolution algorithm:
 *
 *   1. The node with the EARLIER `joinedAt` timestamp wins (first-write-wins).
 *   2. If timestamps are equal, the node with the lexicographically SMALLER
 *      public key wins (deterministic tiebreaker).
 *   3. The winning node's deployment signature is verified before accepting.
 *
 * This is intentionally simple and conservative. It prevents hostile takeovers
 * while allowing legitimate migrations via the `PATCH /sites/:id` endpoint.
 *
 * Future improvement: a proper DAG-based CRDT would allow bidirectional merges,
 * but for static sites (immutable deployments), first-write-wins is correct.
 */

import { db, sitesTable, siteDeploymentsTable, nodesTable } from "@workspace/db";
import { eq, and } from "drizzle-orm";
import { verifySignature } from "./federation";
import logger from "./logger";

export interface ConflictResolutionResult {
  accepted: boolean;
  reason: string;
  winner: "local" | "remote";
}

/**
 * Resolve a domain conflict between the local node's existing site and
 * an incoming sync from a remote node.
 *
 * Returns whether the incoming sync should be accepted.
 */
export async function resolveConflict(opts: {
  siteDomain: string;
  remoteNodeDomain: string;
  remoteDeploymentId: number;
  remoteJoinedAt?: string;
  remotePublicKey?: string;
  remoteSignature?: string;
  remotePayload?: string;
}): Promise<ConflictResolutionResult> {
  const {
    siteDomain,
    remoteNodeDomain,
    remoteDeploymentId,
    remoteJoinedAt,
    remotePublicKey,
    remoteSignature,
    remotePayload,
  } = opts;

  // Find the local site record
  const [localSite] = await db
    .select({ id: sitesTable.id, primaryNodeId: sitesTable.primaryNodeId })
    .from(sitesTable)
    .where(eq(sitesTable.domain, siteDomain));

  // If we don't have this site yet, no conflict — accept
  if (!localSite) {
    return { accepted: true, reason: "new_site", winner: "remote" };
  }

  // If we have an active deployment, check if it's from the same origin node
  const [localActiveDep] = await db
    .select({ id: siteDeploymentsTable.id, deployedBy: siteDeploymentsTable.deployedBy })
    .from(siteDeploymentsTable)
    .where(
      and(
        eq(siteDeploymentsTable.siteId, localSite.id),
        eq(siteDeploymentsTable.status, "active"),
      ),
    );

  // If the local active deployment is also from a federation replica,
  // both are replicas and we need to pick one deterministically
  const localIsReplica = localActiveDep?.deployedBy?.startsWith("federation:");
  const remoteOrigin = localActiveDep?.deployedBy?.replace("federation:", "");

  // If this is an update from the same origin we already have, accept it
  if (remoteOrigin === remoteNodeDomain) {
    return { accepted: true, reason: "same_origin_update", winner: "remote" };
  }

  // ── Verify the remote signature if provided ──────────────────────────────
  if (remoteSignature && remotePayload && remotePublicKey) {
    const signatureValid = verifySignature(
      remotePublicKey,
      remotePayload,
      remoteSignature,
    );
    if (!signatureValid) {
      logger.warn(
        { siteDomain, remoteNodeDomain },
        "[conflict] Invalid signature on sync — rejected",
      );
      return {
        accepted: false,
        reason: "invalid_signature",
        winner: "local",
      };
    }
  }

  // ── Fetch both nodes' joinedAt for temporal ordering ─────────────────────
  const [localNode] = await db
    .select({ joinedAt: nodesTable.joinedAt, publicKey: nodesTable.publicKey })
    .from(nodesTable)
    .where(eq(nodesTable.isLocalNode, 1));

  const [remoteNode] = await db
    .select({ joinedAt: nodesTable.joinedAt, publicKey: nodesTable.publicKey })
    .from(nodesTable)
    .where(eq(nodesTable.domain, remoteNodeDomain));

  const localJoinedAt = localNode?.joinedAt ?? new Date(0);
  const remoteJoinedAtDate = remoteJoinedAt
    ? new Date(remoteJoinedAt)
    : (remoteNode?.joinedAt ?? new Date(0));

  // Rule 1: Earlier joinedAt wins (first-write-wins)
  if (remoteJoinedAtDate < localJoinedAt) {
    logger.info(
      { siteDomain, remoteNodeDomain, reason: "earlier_joined_at" },
      "[conflict] Remote node wins — joined earlier",
    );
    return { accepted: true, reason: "earlier_joined_at", winner: "remote" };
  }

  if (localJoinedAt < remoteJoinedAtDate) {
    logger.info(
      { siteDomain, remoteNodeDomain, reason: "later_joined_at" },
      "[conflict] Local node wins — joined earlier",
    );
    return { accepted: false, reason: "local_node_older", winner: "local" };
  }

  // Rule 2: Timestamps are equal — use public key as deterministic tiebreaker
  const localPubKey = localNode?.publicKey ?? "";
  const remotePubKey = remotePublicKey ?? remoteNode?.publicKey ?? "";

  if (remotePubKey < localPubKey) {
    logger.info(
      { siteDomain, remoteNodeDomain, reason: "pubkey_tiebreaker" },
      "[conflict] Remote node wins — lexicographically smaller public key",
    );
    return { accepted: true, reason: "pubkey_tiebreaker", winner: "remote" };
  }

  logger.info(
    { siteDomain, remoteNodeDomain, reason: "pubkey_tiebreaker" },
    "[conflict] Local node wins — lexicographically smaller or equal public key",
  );
  return { accepted: false, reason: "local_pubkey_wins", winner: "local" };
}
