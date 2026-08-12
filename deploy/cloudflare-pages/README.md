# The apex front door (tnhc.dev)

`tnhc.dev` is a static marketing site on **Cloudflare Pages**. It is not in this
repository and it never reaches this machine's tunnel — that is deliberate: it
stays up when the node is down, which is the one thing a power cut cannot take
away from you.

The consequence is that the apex cannot host a login form. It has no server, no
session, and no path to Auth. Everything that needs one lives on the product
hosts:

| What | Where | Public? |
|---|---|---|
| Sign in | `auth.tnhc.dev/login` | yes |
| Request an account | `app.tnhc.dev/request` | yes |
| Claim an approved account | `app.tnhc.dev/claim` | yes |
| Your apps | `app.tnhc.dev/` | signed in |

So the apex's job is to *point at* those, and this directory holds the two
pieces that do it.

## 1. `_redirects`

Gives the apex the URLs people type and guess — `/login`, `/signup`,
`/register`, `/claim` — and forwards each to the real page.

Copy it into the **root of the Pages project's published output**, beside
`index.html`. For Create React App that is `build/_redirects`; for Vite it is
`public/_redirects` (Vite copies `public/` verbatim). A copy left in the source
root is ignored, and ignored silently — after deploying, confirm with:

```bash
curl -s -o /dev/null -w '%{http_code} %{redirect_url}\n' https://tnhc.dev/login
# expect: 302 https://auth.tnhc.dev/login?redirect_uri=...
```

If that prints `200` you are still being served `index.html` and the file is in
the wrong place.

## 2. `header-cta.html`

The two buttons themselves. A redirect only helps someone who already guessed a
URL; a visitor who just reads the page needs something to click. Paste the
snippet into the site's header or hero.

## What a visitor experiences

1. Lands on `tnhc.dev`, reads the page, clicks **Request access**.
2. Fills in a username and email at `app.tnhc.dev/request` and is shown a claim
   code **once** — there is no confirmation email, and the page says so.
   Outbound mail to the large providers is an IP-reputation wall this project
   deliberately does not depend on.
3. You approve the request in the dashboard's admin panel.
4. They return to `app.tnhc.dev/claim`, enter the code, and set a password.
5. They sign in, and one account now reaches every app in the ecosystem.

Steps 1–5 are verified working end to end as of 2026-08-12.

## If the apex ever moves onto the tunnel

Delete `_redirects` and serve the dashboard at the apex instead — it already
has all four pages. The trade you would be making is availability: today the
apex survives this machine being off, and then it would not.
