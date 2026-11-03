# Node Operators

A Nexus node is a self-hosted machine that joins the federated Nexus Systems
network. Node operators contribute storage and bandwidth to the network and in
return run a full, sovereign copy of the platform on hardware they control.

## What a node does

- Hosts Nexus applications for users on that node
- Participates in federated routing — sites and mail can replicate across peers
- Holds an Ed25519 cryptographic identity that peers use to verify its messages
- Appears in the network's public node directory at `GET /api/v1/nodes`

## What you need

| Requirement | Minimum | Notes |
|-------------|---------|-------|
| Machine | Raspberry Pi 4 / small VPS | Must stay online to serve traffic |
| Domain | Any resolvable hostname | e.g. `node.example.com` — used as the node's identity |
| Storage | 1 GB | Declared at enrollment; more is better |
| Bandwidth | 1 GB/month | Declared at enrollment |
| Network | Outbound port 443 | Inbound is optional (tunnel mode works without it) |
| OS | Linux | The install script is a shell script; Bun is installed automatically |

You do not need a routable public IP. Cloudflare Tunnel or any other reverse
proxy that terminates TLS for your domain works.

## Enrollment flow

Enrollment happens in two steps. The split is intentional: this server never
sees your private key.

### Step 1 — Register (requires a tnhc.dev account)

1. Sign in at `https://app.tnhc.dev`.
2. Go to **Node Operators** (`app.tnhc.dev/node-operators`) and click
   **Enroll a node**, or go directly to `app.tnhc.dev/cloud/federation`.
3. Fill in your node's name, domain, region, and declared capacities.
4. You receive an **enrollment token** — a single-use, 24-hour secret. Copy it
   immediately; it is not stored in a recoverable form.
5. You also receive a ready-to-run install command, e.g.:

   ```sh
   curl -fsSL https://hosting.tnhc.dev/install-node.sh -o install-node.sh \
     && sh install-node.sh --token <your-token> --api https://hosting.tnhc.dev
   ```

### Step 2 — Claim (runs on your machine, no account needed)

Run the install command on the machine that will be the node. The script:

1. Generates an Ed25519 keypair locally — the private key never leaves your machine.
2. Signs the enrollment token with the private key.
3. Calls `POST /nodes/claim` with the token, your public key, and the signature.
4. The server verifies the signature, registers the public key, and activates your node.

Your node moves from `pending` → `active`. From this point peers can discover
and verify it by its registered public key.

### If the token expires

Tokens expire after 24 hours. If you miss the window, ask an operator to revoke
the old token (`POST /nodes/:id/enrollment/revoke`) and issue a new enrollment.

## Regions

Use a short geographic slug that describes where the machine sits, e.g.
`eu-north`, `eu-west`, `us-east`, `us-west`, `ap-southeast`. The value is
informational — routing decisions are not based on it today — but it helps
operators understand the network's topology.

## After enrollment

- Your node appears in `GET /api/v1/nodes` (public federation directory).
- Federation peers will discover and attempt to connect to your node.
- The Cloud console at `app.tnhc.dev/cloud/federation` shows the network's
  current peer list and trust status.
- The node's last-seen timestamp is updated on each successful handshake.

## API reference

| Endpoint | Auth | Description |
|----------|------|-------------|
| `GET /nodes` | None | Public federation directory |
| `POST /nodes/enroll` | Session | Start enrollment; returns single-use token |
| `POST /nodes/claim` | Token (not session) | Finish enrollment with public key + signature |
| `POST /nodes/:id/enrollment/revoke` | Session | Revoke an unclaimed token |

Base URL: `https://hosting.tnhc.dev`

## Questions

Open an issue at `github.com/The-No-Hands-Company/Nexus-Systems` or reach out
directly. Running even a temporary node on a small VPS helps stress-test the
federation protocol — it is appreciated.
