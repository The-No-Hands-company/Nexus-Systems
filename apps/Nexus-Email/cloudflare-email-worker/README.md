# TNHC inbound email Worker

This Worker transfers Cloudflare Email Routing messages for `info@tnhc.dev`
and `zajfan@tnhc.dev` to Nexus Email. It does not change DNS MX records.

## Configure and deploy

1. Generate one random 32-byte token on the Nexus host:

   ```sh
   openssl rand -hex 32
   ```

   Put the same value in `apps/Nexus-Email/.env` (gitignored) as
   `NEXUS_EMAIL_CLOUDFLARE_INGRESS_TOKEN` (`deploy.sh` hands it to mailapi), and
   set it in this Worker with
   `npx wrangler secret put NEXUS_INGRESS_TOKEN`. Do not put the value in this
   repository, `wrangler.toml`, or a message to support.

2. Nothing to add to the tunnel: `email-ingress.tnhc.dev` is covered by the
   `*.tnhc.dev` wildcard, which enters the ecosystem proxy on :8080. The proxy
   pins that host to exactly `POST /internal/v1/cloudflare-email` on
   `127.0.0.1:3140`, skips the login gate (the bearer token is the credential)
   and 404s every other path. Never point a tunnel hostname straight at :3140:
   mailapi's other routes trust a caller-supplied `X-Nexus-Subject`, so a
   direct route would let anyone read any mailbox. `mail.tnhc.dev` is reserved
   for the federation listener on port 2580.

3. Restart `nexus-mailapi`, then from this directory verify the Worker and deploy:

   ```sh
   npm test
   npx wrangler deploy
   ```

4. In Cloudflare, open **Compute → Email Service → Email Routing → Routing
   Rules**. Change only the `info` and `zajfan` rules to **Send to a Worker** and
   select `tnhc-nexus-email-inbound`. Do not add a catch-all rule. Leave all
   three root `@` MX records and unrelated routing rules unchanged.

5. Ensure Nexus has mailboxes/addresses for both recipients with `nexus-mailctl`
   (see the parent README). Send a message to each address and confirm it is
   visible in the corresponding Nexus mailbox. Only after verification should
   you remove any former forwarding destination for those two addresses.

If the API returns an error, the Worker fails the event instead of acknowledging
mail Nexus did not store. Check Worker logs and the Nexus API logs before
retrying. The server endpoint limits ingress to these two local addresses and
requires the shared bearer token.
