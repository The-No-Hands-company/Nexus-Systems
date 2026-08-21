# Email DNS for tnhc.dev

**Status as of 2026-08-21: this domain neither sends nor receives mail, and
publishes nothing saying so — which means anyone can forge `@tnhc.dev` today.**

Two DNS records fix the forgery half. They cost nothing, depend on no third
party, and are true statements about the current setup.

## Add these

| Type | Name | Content | TTL |
|---|---|---|---|
| `TXT` | `tnhc.dev` (or `@`) | `v=spf1 -all` | Auto |
| `TXT` | `_dmarc` | `v=DMARC1; p=reject; sp=reject; adkim=s; aspf=s` | Auto |

Cloudflare → tnhc.dev → DNS → Records → Add record. Leave both **DNS only**
(grey cloud); TXT records are not proxied.

**`v=spf1 -all`** means "no host is authorised to send mail as this domain."
Not "prefer not to" — `-all` is a hard fail. Receivers reject on it.

**`p=reject`** tells receivers to reject anything failing SPF or DKIM alignment
rather than spam-foldering it. `sp=reject` extends that to subdomains, so
`billing.tnhc.dev` cannot be forged either. `adkim=s`/`aspf=s` require strict
alignment rather than relaxed.

No `rua=` reporting address: aggregate reports arrive by email, and this domain
cannot receive email. Pointing them somewhere unreadable would be decoration.

## Why there is no MX record

Deliberate, not an oversight — but also not permanent.

Adding an MX is only useful if something can answer on port 25 from the public
internet. Nothing can: the mail service listens on `127.0.0.1:2525` and
`127.0.0.1:2587` (localhost, non-standard ports), inbound `:25` to this
connection is unreachable, and the Cloudflare Tunnel carries HTTP/HTTPS only —
every published route in it is an `http://` upstream. There is no SMTP path in.

An MX pointing at a host that cannot answer produces retry storms at the
sender and bounces that look like our fault. Publishing nothing is more honest
than publishing a promise nothing keeps.

Cloudflare's dashboard warns about the missing MX. It is right that mail cannot
reach `@tnhc.dev`; it is wrong that an MX record alone would fix it.

## Why outbound mail does not work, and cannot be made to

Measured 2026-08-21, not assumed:

- **IPv4 `:25` outbound is blocked** by the ISP. Port 25 is the only port
  receiving mail servers listen on for server-to-server delivery.
- **No IPv6 at all**, so there is no `v6:25` path around the block.
- **`:587` and `:465` are open**, but those are submission ports — they reach a
  *relay*, which is a third-party service.

Even with `:25` unblocked, two further walls stand: Spamhaus PBL lists
residential ranges by design and Gmail consults it, and delivery needs a PTR
record matching the sending hostname, which the ISP controls and does not
generally offer on a residential line.

This is not a code problem. No amount of self-hosted engineering routes around
a blocked port and a residential-range blocklist.

## What to do instead

Do not build flows that depend on outbound mail. The one that already did —
the 30-day email-verification gate on deploys — was a timer that made every
non-admin account permanently unable to deploy, with the only stated remedy
impossible to perform. It is now conditional on `SMTP_HOST` being set, so it
returns by itself if delivery ever becomes possible.

Prefer in-app channels: show an invite code in the UI rather than mailing it,
report issues through a signed-in form rather than an email address. Those work
today, need no third party, and do not decay.

## If outbound mail is ever genuinely needed

The only routes are, in order of preference:

1. **Ask the ISP to unblock `:25` and provide a PTR record.** Free, no third
   party. Likely refused on a residential plan, and still leaves the PBL
   listing.
2. **A relay on `:587`.** Works immediately, but is a third-party dependency
   and therefore against the self-sufficiency principle.

If either happens, replace `v=spf1 -all` with a record naming the authorised
sender, add a DKIM key, and lower DMARC to `p=quarantine` while checking
alignment before returning it to `p=reject`.
