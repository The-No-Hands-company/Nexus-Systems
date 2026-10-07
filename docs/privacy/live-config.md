# Live configuration changes (zero retention)

## Tunnel ingress

Changed 2026-10-07 via the Cloudflare tunnel configuration API (tunnel
`a3fc7587-49de-4792-b532-882775db6457`). Two rules now point at the proxy
(`http://192.168.0.179:8080`) instead of bypassing it, so address stripping
applies to them:

| Hostname | Before | After |
|----------|--------|-------|
| `auth.tnhc.dev` (no path rule) | `http://172.17.0.1:8000` (Supabase gateway) | `http://192.168.0.179:8080` |
| `storage.tnhc.dev` | `http://192.168.0.179:9010` (MinIO) | `http://192.168.0.179:8080` |

The proxy sends non-login `auth.tnhc.dev` paths to `SUPABASE_UPSTREAM` and
`storage.tnhc.dev` to `STORAGE_UPSTREAM`, preserving the original `Host` for
storage so presigned signatures verify.

### Rollback

Put the previous configuration back (the saved copy is
`/tmp/claude-ingress-before.json`; if it is gone, revert the two services above
by hand in the same payload shape):

```bash
A=fe2435f2045c5a1cc39c0b5f78fc92c7; T=a3fc7587-49de-4792-b532-882775db6457
CF=$(sed -n 's/^CF_API_TOKEN=//p' apps/Nexus-Cloud/.env | tr -d '"\r')
jq '{config: .result.config}' /tmp/claude-ingress-before.json |
  curl -s -X PUT -H "Authorization: Bearer $CF" -H 'Content-Type: application/json' \
    --data @- https://api.cloudflare.com/client/v4/accounts/$A/cfd_tunnel/$T/configurations | jq .success
```

## Supabase

Live self-hosted Supabase (`projects/TNHC-Community-deployment`, containers
`supabase-*`). Files there are edited in place and are NOT committed (its git
remote is this repo).

1. **Auth tables hold no client addresses.** `docs/privacy/supabase-zero-retention.sql`
   wipes and then forces, with BEFORE INSERT OR UPDATE triggers
   (functions `public.tnhc_blank_*`):
   `auth.audit_log_entries.ip_address` = `''`; `auth.sessions.ip` and
   `auth.sessions.user_agent` = NULL; `auth.mfa_challenges.ip_address` = `0.0.0.0`.
   Applied: `docker exec -i supabase-db psql -U postgres < docs/privacy/supabase-zero-retention.sql`
   (4 session rows wiped). Revert (stop forcing; wiped data is gone):
   `DROP TRIGGER tnhc_blank_audit_ip ON auth.audit_log_entries; DROP TRIGGER tnhc_blank_session_client ON auth.sessions; DROP TRIGGER tnhc_blank_mfa_ip ON auth.mfa_challenges;`
2. **Gateway access log off.** `volumes/api/envoy/lds.template.yaml`: deleted the
   `access_log:` block (was lines 26-33, logged remote address + user agent).
   `docker-compose.yml`: added to service `api-gw` (container `supabase-envoy`)
   `logging: { driver: "none" }` (lines 73-74). Recreated only that service:
   `docker compose -f docker-compose.yml -f docker-compose.override.yml -f /run/media/zajferx/Data/dev/The-No-hands-Company/projects/TNHC-Community-deployment/docker-compose.ext4-volumes.yml up -d --no-deps --force-recreate api-gw`
   (the config files the container was created with). Revert: re-add the block /
   remove the `logging` lines and recreate the same way.
3. **Dev stack log collection stopped.** `docker update --restart=no` and
   `docker stop` on `supabase_analytics_tnhc-community-local` and
   `supabase_vector_tnhc-community-local`. Revert: `docker update --restart=unless-stopped <name>; docker start <name>`.

### Supabase: durable recreate procedure and revert sources

The running gateway was created with three compose files. The third was
originally in `/tmp` (lost on reboot); it now lives, uncommitted, at
`/run/media/zajferx/Data/dev/The-No-hands-Company/projects/TNHC-Community-deployment/docker-compose.ext4-volumes.yml` (a verbatim copy, no secrets):

```yaml
services:
  db:
    volumes:
      - db-data:/var/lib/postgresql/data
  storage:
    volumes:
      - storage-data:/var/lib/storage
  imgproxy:
    volumes:
      - storage-data:/var/lib/storage
volumes:
  db-data:
  storage-data:
```

Recreate from the deployment dir:
`docker compose -f docker-compose.yml -f docker-compose.override.yml -f /run/media/zajferx/Data/dev/The-No-hands-Company/projects/TNHC-Community-deployment/docker-compose.ext4-volumes.yml up -d --no-deps --force-recreate api-gw`

`/tmp/dc.bak` and `/tmp/lds.bak` (pre-edit copies) are temporary; the
before/after text in this section is the revert source.

### After a Supabase/GoTrue upgrade

GoTrue migrations that ALTER/DROP these columns fail while the triggers depend
on them, and a table rebuild drops the triggers silently. So: before upgrading,
drop the 3 triggers (`tnhc_blank_audit_ip`, `tnhc_blank_session_client`,
`tnhc_blank_mfa_ip`, commands in item 1 above); after upgrading, re-run
`docs/privacy/supabase-zero-retention.sql`; then verify this returns 0,0,0:

```sql
SELECT
  (SELECT count(*) FROM auth.audit_log_entries WHERE ip_address <> '') AS audit_nonblank,
  (SELECT count(*) FROM auth.sessions WHERE ip IS NOT NULL OR user_agent IS NOT NULL) AS sessions_with_client,
  (SELECT count(*) FROM auth.mfa_challenges WHERE ip_address <> '0.0.0.0') AS mfa_not_zero;
```
