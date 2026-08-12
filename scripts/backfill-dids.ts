export type UserRecord = { id: string; email?: string | null; username?: string; phantom_did?: string | null };

export type DBClient = {
  // return users with phantom_did null or undefined
  getUsersWithoutDid: () => Promise<UserRecord[]>;
  // update user phantom_did
  updateUserDid: (id: string, did: string) => Promise<void>;
};

export type BackfillOptions = {
  db: DBClient;
  didMapperUrl?: string; // base url
  didMapperKey?: string | undefined;
  rate?: number; // per second
  didGenerator?: (u: UserRecord) => string;
  logger?: (msg: string) => void;
};

function defaultDidGenerator(u: UserRecord) {
  // deterministic DID for backfill: did:phantom:<user-id>
  return `did:phantom:${u.id}`;
}

export async function runBackfill(opts: BackfillOptions) {
  const base = opts.didMapperUrl || process.env.DID_MAPPER_URL || 'http://localhost:4001';
  const key = opts.didMapperKey ?? process.env.DID_MAPPER_KEY;
  const rate = opts.rate ?? (process.env.BACKFILL_RATE ? Number(process.env.BACKFILL_RATE) : 50);
  const gen = opts.didGenerator || defaultDidGenerator;
  const log = opts.logger || (() => {});

  const users = await opts.db.getUsersWithoutDid();
  log(`Found ${users.length} users without phantom_did`);
  if (users.length === 0) return { processed: 0 };

  const intervalMs = Math.max(1, Math.floor(1000 / Math.max(1, rate)));

  let processed = 0;

  for (let i = 0; i < users.length; i++) {
    const u = users[i];
    // idempotency: skip if phantom_did present
    if (u.phantom_did) continue;
    const did = gen(u);

    // POST mapping
    const body = JSON.stringify({ did, user_id: u.id });
    const headers: Record<string,string> = { 'content-type': 'application/json' };
    if (key) headers['x-api-key'] = key;

    try {
      const res = await fetch(`${base.replace(/\/$/, '')}/v1/dids`, {
        method: 'POST',
        headers,
        body
      });
      if (!res.ok && res.status !== 201) {
        log(`Failed to create mapping for ${u.id} -> ${did}: ${res.status}`);
        // do not update user on failure
      } else {
        await opts.db.updateUserDid(u.id, did);
        processed++;
      }
    } catch (err) {
      log(`Error creating mapping for ${u.id}: ${String(err)}`);
    }

    // rate-limit delay except after last
    if (i < users.length - 1) {
      await new Promise((r) => setTimeout(r, intervalMs));
    }
  }

  return { processed };
}

// CLI
if (import.meta.main) {
  (async () => {
    // Simple SQLite / Postgres integration not implemented here.
    console.log('Running backfill-dids script (dry-run by default). Provide DB client via programmatic API for real runs.');
  })();
}
