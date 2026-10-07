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
