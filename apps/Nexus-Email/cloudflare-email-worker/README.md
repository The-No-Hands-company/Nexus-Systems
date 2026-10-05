# TNHC inbound email Worker

This Worker transfers Cloudflare Email Routing messages for `info@tnhc.dev`
and `zajfan@tnhc.dev` to Nexus Email. It does not change DNS MX records.

## Configure and deploy

1. Generate one random 32-byte token on the Nexus host:

   ```sh
   openssl rand -hex 32
   ```

   Put the same value in the `nexus-mailapi` environment as
   `NEXUS_EMAIL_CLOUDFLARE_INGRESS_TOKEN` and set it in this Worker with
   `npx wrangler secret put NEXUS_INGRESS_TOKEN`. Do not put the value in this
   repository, `wrangler.toml`, or a message to support.

2. Add an ingress hostname to the existing Cloudflare Tunnel:
   `email-ingress.tnhc.dev` → `http://127.0.0.1:3140`. This hostname must route
   to `nexus-mailapi`; `mail.tnhc.dev` is reserved for the federation listener
   on port 2580. Keep the API bound to loopback.

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
