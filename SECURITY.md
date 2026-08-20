# Security policy

## Reporting a vulnerability

**[Open a private advisory](https://github.com/The-No-Hands-company/Nexus-Systems/security/advisories/new)**
for anything exploitable. Only the maintainers can read it, and it stays
private until there is a fix to announce.

Please do not open a public issue for a vulnerability. Public issues are the
right place for every other kind of problem — see below.

Machine-readable version: [`/.well-known/security.txt`](https://tnhc.dev/.well-known/security.txt)

## What is in scope

- `tnhc.dev` and every `*.tnhc.dev` host
- Any repository under [github.com/The-No-Hands-company](https://github.com/The-No-Hands-company)

The ecosystem is a set of submodules. A report against any of them belongs
here — you do not need to work out which repository owns the code.

## What we especially want to hear about

Every line of this project is written by AI agents. That is the whole premise,
and it is also the reason outside review matters more here than it would on a
hand-written codebase: the failure modes are different. Confident, plausible,
well-commented code that is wrong is the characteristic bug, and it does not
look like a bug from the inside.

Things worth reporting even if you are not certain:

- An endpoint that returns more than it should — personal data, internal
  addresses, keys, or fields a caller has no business seeing. Two such leaks
  were found and fixed in August 2026; assume there are more.
- Anything reachable without authentication that reads as though it expects
  authentication.
- A document, README, or page on the site that claims something the code does
  not actually do. We treat these as bugs, not marketing. Several have been
  corrected for exactly this reason.
- Cryptographic code that looks right but is not. `apps/Nexus` implements X3DH
  and `apps/Phantom` implements FHE and zero-knowledge routing. Both are areas
  where a confident implementation can be confidently wrong.

## What happens next

There is no bug bounty. The entire recurring cost of this project is about
twelve dollars a year, and there is no revenue behind it.

What you get instead: a reply, the fix landed and visible in the public
changelog at [tnhc.dev/changelog](https://tnhc.dev/changelog), and credit
under whatever name you want if you want any.

## Everything else

Bugs, broken pages, wrong copy, missing features, and things that are merely
strange all go to
**[Issues](https://github.com/The-No-Hands-company/Nexus-Systems/issues)**.

A report that turns out to be nothing costs us a few minutes. A problem nobody
mentioned costs a lot more.
