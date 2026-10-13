# Emailed invites and approval notices — design

Date: 2026-10-13. Status: proposed.

## Goal

An operator types an email address in the Admin page and the person receives an
invitation by email. When an operator approves an access request, the person is
told by email that they can claim their account. Today both steps are manual: the
operator copies a link or code and sends it through some other channel.

## Constraints

- **Self-contained.** No new third-party service. Email leaves through whatever
  egress Nexus-Email is already configured with; this feature adds none.
- **Email is an enhancement, never a dependency.** Every flow keeps working with no
  outbound email at all (`nexus-self-sufficiency-principle`). If sending fails or is
  not configured, the operator still gets the link to hand over.
- **Privacy.** Store as little as possible, for as short a time as possible.
- **Industry-standard shape.** Same as GitHub, Google Workspace and AWS IAM invites:
  a single-use, expiring link to the identity host, bound to the invited address.

## Third-party touchpoints and costs

| Touchpoint | Why | Cost | Self-contained replacement |
|---|---|---|---|
| Resend (current Nexus-Email egress) | This ISP blocks outbound port 25 (verified 2026-10-13: connection to Gmail's MX on :25 times out), so mail to outside addresses cannot be sent directly | Free plan: 100 emails/day, about 3,000/month. Over the cap, sending pauses; no automatic charge. Next tier is $20/month | Nexus-Email's `peer:<domain>` egress: hand outbound mail to a federated Nexus node that can deliver. No such node exists yet |
| Resend sees each message | It relays it | n/a | Same as above |

This feature does not add a touchpoint. It uses the egress that already exists,
and switches to a peer automatically when the egress setting changes. System mail
is capped well below the free plan's daily limit (below), so invites can never
exhaust the quota that people's own mail depends on.

What Resend sees for an invite: the recipient address, the subject, and the body,
which contains the invite link. Mitigations: the link expires in 7 days, works
once, and only for the invited address.

## Components

### 1. Nexus-Auth

**Email-bound invites.** `createInvite` gains an optional `email`. When present:

- the invite stores `emailHash` = SHA-256 of the normalised (trimmed, lowercased)
  address, never the address itself;
- the expiry is 7 days instead of 30;
- `redeemInvite` refuses unless the redeeming email hashes to `emailHash`
  (reason `invalid_code`, the same message as a wrong code, so the page reveals
  nothing about which part was wrong).

**Sending.** `POST /api/v1/auth/invites` accepts `{ email?, days? }`. With an email it
creates a bound invite, then asks Nexus-Email to send it. It returns
`{ id, code, expiresAt, link, delivery }`, where `delivery` is one of `sent`
(queued for delivery), `failed`, `disabled` (no mail service configured) or
`not_requested`. The code and link are returned in every case, shown once, as the
fallback.

**Approval notices.** Approving an access request asks Nexus-Email to send a notice
linking to `/claim?email=…`. The claim code is never in this email: it was shown
once when the person asked, and is stored only as a hash. A failed notice does not
fail the approval.

**Rate limit.** At most 20 invites per operator per day, which bounds what a stolen
operator session could send.

### 2. Nexus-Email: a system-mail endpoint

`POST /internal/v1/system-mail`, bound to loopback only and never routed by the
proxy (the proxy allow-lists `email-ingress.<domain>` to a single other path, so
this one is unreachable from outside). The caller must present a service token
in `Authorization: Bearer`, checked in constant time, the same pattern as the
Cloudflare ingress token. Body:

```json
{ "template": "invite" | "access-approved", "to": "person@example.com",
  "link": "https://auth.tnhc.dev/join?code=…", "expiresAt": "2026-10-20T…Z" }
```

- **Fixed templates only.** Subject and body are composed by Nexus-Email from the
  template name. The caller supplies no free text, so the endpoint cannot be
  turned into a spam relay.
- **The link is validated.** It must start with `https://auth.<primary domain>/`
  and use the template's path (`/join` or `/claim`); anything else is refused.
- **Sender.** `noreply@<primary domain>`, a system mailbox with no inbox.
- **Daily cap.** 50 system messages per day by default, configurable.
- **Retention.** Once the message is delivered, or fails permanently, its stored
  copy and its queue row are deleted. Nothing about an invite remains in
  Nexus-Email afterwards.

### 3. Dashboard: Admin page

The invite card gains an email field and two actions: **Send invite** and
**Create link only**. The result shows the outcome in plain words ("Invite sent
to ada@example.com", "Could not send the email. Send this link yourself:") and
always shows the link once.

### 4. Deploy

`deploy.sh` generates `NEXUS_SYSTEM_MAIL_TOKEN` once if it is missing, stores it in
the two services' `.env` files and passes it via the environment, never via argv,
the same as the existing ingress token.

## Tests (written first)

- Auth: bound invite rejects a different email and accepts the right one in any
  case or spacing; unbound invites behave as today; emailed invites expire in 7
  days; delivery outcomes `sent`, `failed`, `disabled` map correctly against a fake
  mail service; the per-operator cap; approval sends a notice and still approves
  when the notice fails.
- Nexus-Email: missing or wrong token gives 401; unknown template, a link on
  another host or path, and a malformed recipient each give 400; the daily cap
  gives 429; a valid request enqueues one message from `noreply@`; delivered
  system mail is purged.
- Proxy: `/internal/v1/system-mail` is unreachable through `email-ingress.<domain>`.
- Dashboard: Send invite shows "sent" with the link; a failed send shows the link
  as the fallback.

## Out of scope

- A federated delivery peer (the self-contained replacement for Resend). That is
  its own design; this feature works unchanged when it lands.
- Retention of ordinary delivered mail in `outbound_queue` (recipient addresses
  are currently kept indefinitely). Recorded as an audit finding.
