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
format) at `refs/heads/main` *as it was before the push*; if that file is
absent, the repository's **trust root**, set when it was created. The policy
ref is fixed: it is never read from `HEAD`, so repointing `HEAD` at a branch
carrying an unreviewed policy change gains nothing.

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
`{seq, push, time, ref, old, new, pusher, prev, hash}`, where `hash` is SHA-256
over the other fields, `prev` is the previous entry's hash and `push` is the
`seq` of the first entry the same push wrote (a push is judged as a whole, so
replaying the policy needs its boundaries).

## Enforcement on the client

`forge verify <url> [--trust-root <sha256:fingerprint | file>]` does not
depend on the server having run its hook. It treats everything the server
sends as untrusted input, keeps a mirror of the repository
(`$XDG_CACHE_HOME/nexus-forge/mirrors/`), and:

1. checks the served trust root against the fingerprint the repository owner
   gave out of band (`forge admin repo create` prints it), or the one pinned
   on an earlier run (`$XDG_CONFIG_HOME/nexus-forge/verified.json`). **A
   first run with neither refuses** and shows the served fingerprint to
   compare; accepting it unchecked needs an explicit `--trust-on-first-use`
2. checks the ref log: every entry well-formed (object ids are hex, ref names
   plain, no extra fields, so nothing from the log can become a git option),
   every link intact, and the head verified last time still present
3. checks the log replays to exactly the refs the mirror fetched
4. replays **every push in the log** through the same `checkUpdates()` the
   server's hook runs, with the state before each push rebuilt from the log;
   pushes verified on an earlier run are skipped (2 proves them unchanged)

**Plain git, verified: `git-remote-nexus`.** After `forge install-helper`,
`git clone -c nexus.trustRoot=sha256:… nexus::https://host/repo.git` and
every later `git fetch` / `git pull` run all of the above first and abort
if it fails. Objects are copied from the verified mirror, never straight
from the server, so a clone only ever contains what was verified.

The client's git only speaks http(s) (no `ext::`, `file://` or ssh command
transports), follows no redirects (one could hand the token to another
host), fsck's every fetched object, and refuses URLs with credentials in
them (they would be written to disk) and plain http to other machines.

The tests run a compromised forge (its pre-receive accepts everything, the
log is still written): unsigned and wrongly signed commits, a multi-ref push
that uses a key it just added, a swapped trust root and a rewritten log are
all refused, and a `git pull` through the helper leaves the clone untouched.

## Hooks fail closed

git skips a hook that is not executable and accepts the push. The forge
therefore generates its hooks at startup (mode 0755, in
`<storage>/.forge-hooks`), runs each once before serving, refuses to start if
they do not run, and re-checks before every push (503 if they stopped being
executable). Checked-in hook scripts arrived as mode 100644 on a fresh
checkout, which would have accepted every push unchecked.

## Resource limits on the server

- push bodies are streamed into git with a byte cap (512 MiB default): a
  declared oversize body is refused before reading, a streamed one is cut
  off and git refuses the truncated pack; nothing is buffered in memory
- at most 16 git processes at once (configurable); past that, 503 with
  `Retry-After` instead of an unbounded queue
- the signature check is one git process per ref and stops at the first bad
  commit, so a push of many unsigned commits costs one process
- every response carries `nosniff`, `default-src 'none'; frame-ancestors
  'none'` and `no-referrer` (the forge serves no HTML)
- usernames and repository names come from allowlists: both reach git's
  environment, paths or the ref log

## Limits

- **A clone made without `nexus::` is not verified.** Plain `https://`
  remotes check nothing; `forge verify` can still be run on demand.
- **A client that has never pinned a head cannot detect a consistent rewrite.**
  Closing that needs heads witnessed by someone other than the server, such as
  federation peers.
- **No server signature on log entries yet.** The chain proves internal
  consistency, not who wrote it.
- **Commits are SHA-1 by default.** git's SHA-1 has collision detection, but
  SHA-256 repositories (`--object-format=sha256`) are the stronger choice;
  supporting them end to end is open.
