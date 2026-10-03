# Nexus Forge security model

The goal: compromising the forge's server should not let an attacker change
what a repository's users accept as its history. This first slice gets part
of the way; the limits are listed as plainly as the guarantees.

## Who can do what

- **Access** is decided once, in `authorize()` (`src/backend/auth/access.ts`).
  Default-deny. Anonymous may read public repositories; anything else needs a
  per-repository grant. Unauthenticated requests for private *or missing*
  repositories get the same 401; signed-in strangers get the same 404.
- **Tokens** expire (8h default, 30 days max), are stored only as SHA-256, and
  can be revoked per user. There is no login endpoint; tokens are issued by
  `forge admin token`, which needs filesystem access to the forge's database.

## What may be pushed

The pre-receive hook (`src/backend/policy/verify.ts`) refuses the whole push
unless:

1. Only `refs/heads/*` and `refs/tags/*` are updated, nothing is deleted, and
   no branch is rewound (git's own `denyDeletes` / `denyNonFastForwards`, set
   on the command line).
2. Every commit the push introduces, on any ref, has an **SSH signature** from
   a key in the **policy in force**. Annotated tags must be signed too.
   OpenPGP and X.509 signatures never verify.
3. A push that changes `.nexus/allowed_signers` on the default branch leaves it
   parseable.

**Policy in force** = `.nexus/allowed_signers` (git's `allowedSignersFile`
format) at the default branch's tip *as it was before the push*; if that file
is absent, the repository's **trust root**, set when it was created.

Reading the policy from the pre-push tip, not from each commit's parent:

| Attack | Result |
|---|---|
| Commit adds its own signer to the policy | Refused: its content is not in force yet |
| Push adds a key, then a later commit in the same push uses it | Refused |
| Key revoked on main; attacker branches from before the revocation | Refused: the old policy is not revived |
| Signed merge whose second parent is unsigned | Refused: every new commit is checked |

Cost: a policy change must land in its own push before the new key can sign.

Everything fails closed: a missing trust root, missing hook environment, an
unparseable policy or a crash in the check refuses the push.

## What the server did: the ref log

Each accepted update is appended to `<repo>.git/nexus/ref-log.jsonl` as
`{seq, time, ref, old, new, pusher, prev, hash}`, where `hash` is SHA-256 over
the other fields and `prev` is the previous entry's hash.

`forge log verify <url>` checks that:

- every link holds (an edited, dropped or reordered entry fails)
- the log replays to exactly the refs the server serves (a ref moved outside a
  push fails, and so does a push whose log append failed)
- the log still contains the head this client verified last time, pinned in
  `$XDG_CONFIG_HOME/nexus-forge/pins.json` (a consistent rewrite of the whole
  chain fails *for a client that had pinned it*)

## Limits of this slice

- **The server still enforces the policy.** A compromised server can skip the
  hook. Signatures on the commits survive that, but clients do not yet check
  them on fetch. Next: a client-side `forge verify` that replays the same
  policy over fetched history, so enforcement no longer depends on the server.
- **The trust root lives on the server.** A compromised server could replace
  it, and a repository with no `.nexus/allowed_signers` yet would then accept
  whatever the replacement allows. Committing the policy file early limits
  this; clients should also pin the trust root (or its hash) out of band.
- **A client that has never pinned a head cannot detect a consistent rewrite.**
  Closing that needs heads witnessed by someone other than the server, such as
  federation peers.
- **No server signature on log entries yet.** The chain proves internal
  consistency, not who wrote it.
- Pushes are buffered in memory (512 MiB cap). Fine for a single machine;
  streaming is a later change.
