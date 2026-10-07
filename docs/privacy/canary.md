# Privacy canary

A daily check that proves, rather than asserts, that TNHC keeps no visitor addresses.

## What it does

`scripts/privacy-canary.sh` picks a unique address from the documentation range `203.0.113.0/24` plus a unique token. It sends requests only to the front door (`127.0.0.1:8080`) with `Host` headers for auth, app, cloud, chat, hosting, storage, draw and email-ingress. Each request carries the address in `CF-Connecting-IP` and `X-Forwarded-For`, and the token in the User-Agent and the query string. It also posts a login for a user that does not exist, and an unauthenticated post to the email ingress (which must answer 401).

It then searches every place data could land for the address or token:

- native service logs in `/tmp/nexus-production/*.log` and `*.log.1`
- `docker logs --since 10m` of every running container
- a data-only dump of every production database (`nexus`, `nexus_chat`, `nexus_cloud`, `nexus_email` on the main Postgres, Hosting's `nexus`, Supabase's `postgres`), piped straight into grep and never written to disk
- Auth's JSON stores and Terminal's audit SQLite

A database dump that fails counts as a failure, since it cannot prove anything.

## Result

The latest result is `/tmp/nexus-production/privacy-canary.json` (tmpfs):

```json
{"at":"2026-10-07T16:00:00Z","address":"203.0.113.77","status":"pass","found":[]}
```

`status` is `pass` or `fail`; on `fail`, `found` names each location (for example `db:nexus-systems-postgres-1/nexus_chat`), and the script exits 1.

## Schedule and tests

`deploy.sh` installs a user systemd timer (`nexus-privacy-canary.timer`, `OnCalendar=daily`, `Persistent=true`). Run it by hand with `scripts/privacy-canary.sh`.

`scripts/privacy-canary.sh --self-test` plants the marker in a temporary log directory and confirms the search reports it, which proves the search can fail. `deploy/production/tests/privacy-canary.test.sh` runs it.
