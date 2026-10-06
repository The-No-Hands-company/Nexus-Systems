# TNHC Charter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish the TNHC Charter from a new `handbook` repo, render it (and a Phantom status page) on tnhc.dev, link it from every TNHC surface, and make every public TNHC repo's licence match it.

**Architecture:** `handbook` is the single source of truth (Markdown, CC BY 4.0). tnhc.dev gains two generators in its existing derive-don't-copy pattern (`scripts/build-*.py` → committed `frontend/src/data/*.js`) that render `charter.md` and Phantom's `STATUS.md` at a recorded commit. Licences are applied repo by repo, with a checker script in Nexus-Systems guarding the per-directory split.

**Tech Stack:** Markdown; Python 3 + `markdown` 3.10 + pytest (tnhc.dev generators); React 19 / CRA + react-router (tnhc.dev); React + Vitest (dashboard); Astro (zajfan.dev); Kotlin / Jetpack Compose (tnhc-community); Rust/Cargo (Phantom); bash; `gh` CLI.

**Spec:** `docs/superpowers/specs/2026-10-06-tnhc-charter-design.md`

## Global Constraints

- Platform code (apps, services, sites): `AGPL-3.0-or-later`.
- Libraries, SDKs, protocol crates, design system (`packages/*`, Phantom): `Apache-2.0`. Phantom keeps its existing `MIT OR Apache-2.0` dual licence (permissive, the Rust norm, already in its `Cargo.toml`).
- Documents, handbook, brand assets: `CC-BY-4.0`.
- Copyright holder wording in licence notices: `The No Hands Company contributors`.
- No `All rights reserved` and no `™` in any TNHC-authored file.
- Every Charter/status claim must be true against the code; unfinished work is labelled not done.
- Canonical Charter URL: `https://tnhc.dev/charter`. Phantom status URL: `https://tnhc.dev/phantom`.
- Contact address for reports and the Code of Conduct: `info@tnhc.dev` (live, routed into Nexus Email).
- Never relicense third-party code: any existing `LICENSE*`/`COPYING*` file below a repo root stays untouched.
- Before adding a licence to a repo, list its commit authors; if anyone other than the founder (Zajfan / Eric Håkansson / zajferx) and AI co-authors appears, stop and ask the user.
- Licence texts are downloaded verbatim, never retyped: AGPL `https://www.gnu.org/licenses/agpl-3.0.txt`, Apache `https://www.apache.org/licenses/LICENSE-2.0.txt`, CC BY `https://creativecommons.org/licenses/by/4.0/legalcode.txt`, Contributor Covenant 2.1 `https://www.contributor-covenant.org/version/2/1/code_of_conduct/code_of_conduct.md`.
- Commit messages: `type: description`, ending with the Co-Authored-By line the session specifies. Push each repo's `main` directly (solo-dev convention).
- Work directory for fresh clones: `~/tnhc-charter-work` (home volume — fast; not the NTFS data volume).
- **Do not commit inside `/run/media/zajferx/Data/dev/The-No-hands-Company` (the org folder itself):** its `.git` is wired to the Nexus-Systems remote; a push from there would land in the wrong repository.
- The dashboard build in the main Nexus-Systems checkout **is** the production deploy (`apps/Nexus-Dashboard/frontend/dist` is served live). Build there only in the deploy step of Task 6.

---

### Task 1: Handbook repo — Charter, Code of Conduct, licence

**Files (repo `The-No-Hands-company/handbook`, cloned to `~/tnhc-charter-work/handbook`):**
- Create: `charter.md`, `CODE_OF_CONDUCT.md`, `LICENSE`, `README.md`, `finances.md`
- Create: `scripts/check-charter.sh` (structural check)

**Interfaces:**
- Produces: `charter.md` at the repo root on `main` (Task 5 reads it via `git show <sha>:charter.md`). The file's first line is `# The No Hands Company Charter`. Links inside it are absolute `https://` URLs (they are rendered on tnhc.dev, not GitHub).

- [ ] **Step 1: Clone the empty repo**

```bash
mkdir -p ~/tnhc-charter-work && cd ~/tnhc-charter-work
gh repo clone The-No-Hands-company/handbook
cd handbook && git checkout -b main
```

- [ ] **Step 2: Write the failing structural check**

`scripts/check-charter.sh`:

```bash
#!/usr/bin/env bash
# Fails when the Charter is missing a required section, carries a placeholder,
# or contradicts the "nobody owns this" licensing.
set -euo pipefail
cd "$(dirname "$0")/.."
f=charter.md
[ -f "$f" ] || { echo "FAIL: $f missing"; exit 1; }
head -1 "$f" | grep -qx '# The No Hands Company Charter' || { echo "FAIL: title"; exit 1; }
for h in '## Our promises' '## Ownership and licences' '## Money' '## How we build' '## Who decides' '## Taking part'; do
  grep -qx "$h" "$f" || { echo "FAIL: missing section: $h"; exit 1; }
done
for bad in 'TODO' 'TBD' 'All rights reserved' '™'; do
  if grep -rIl --exclude-dir=.git --exclude=check-charter.sh -- "$bad" . >/dev/null; then echo "FAIL: found '$bad'"; exit 1; fi
done
grep -q 'https://tnhc.dev/phantom' "$f" || { echo "FAIL: Phantom status link missing"; exit 1; }
[ -f CODE_OF_CONDUCT.md ] && grep -q 'info@tnhc.dev' CODE_OF_CONDUCT.md || { echo "FAIL: Code of Conduct contact"; exit 1; }
grep -q 'Attribution 4.0 International' LICENSE || { echo "FAIL: LICENSE is not CC BY 4.0"; exit 1; }
echo PASS
```

- [ ] **Step 3: Run it to see it fail**

Run: `chmod +x scripts/check-charter.sh && scripts/check-charter.sh`
Expected: `FAIL: charter.md missing`

- [ ] **Step 4: Write `charter.md`** (exact text — the user approves wording at plan review)

```markdown
# The No Hands Company Charter

The No Hands Company (TNHC) offers, for free, the kind of software and services others charge for — self-hosted, federated and open to everyone.

This charter is what we promise and how we work. Every sentence in it should be true when you check it against our code. If one isn't, that is a bug: please report it.

## Our promises

- Everything we make is open source.
- Nothing is sold, and nothing is behind a paywall.
- Nothing locks you in. You can take your data and leave, or run everything yourself.

### Your data

- We collect no data of any kind: no tracking, no telemetry, no analytics, no profiling.
- You can export your data at any time, in open formats.
- You can move to your own node at any time.
- When you delete your account, your data is deleted.

We are building the Phantom Protocol to enforce this with cryptography, so that even TNHC's own infrastructure cannot see what you do. It is not finished. The [Phantom status page](https://tnhc.dev/phantom) shows exactly what works today and what does not, and we never claim more than it shows.

### Availability

TNHC's free public services run on our own hardware, on a best-effort basis. There is no uptime guarantee and no guaranteed support. If you need guarantees, run your own node or use someone else's — that is what self-hosting and federation are for.

## Ownership and licences

Nobody owns TNHC's work more than anyone else. Everything we make is licensed so that you can use it, change it and share it:

| What | Licence |
| --- | --- |
| Applications and services (the Nexus platform, our sites) | [AGPL-3.0](https://www.gnu.org/licenses/agpl-3.0.html) |
| Libraries, SDKs, protocols and the design system | [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) |
| Documents, this handbook, and brand assets | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |

The AGPL keeps the platform free: anyone may run a changed version, but if they offer it to others as a service, they must share their changes too. The building blocks are licensed permissively so that anyone can build on them.

### Our name and logo

You may fork, change and run anything we make, under any name you choose. You may use our name and logo to talk about TNHC — in articles and talks, or to say that something works with Nexus.

What you may not do is present something as TNHC, or as made or endorsed by TNHC, when it is not. This protects people from impostor services that could steal their accounts. It does not stop anyone from building on our work.

## Money

We do not sell anything, so the only way to give us money is a donation. We never ask for donations and nothing ever requires one. A donation buys no features, no priority and no say in decisions.

Every donation we receive and every expense we pay is published in our [finances](https://github.com/The-No-Hands-company/handbook/blob/main/finances.md).

Anyone running their own Nexus node may charge for their own hosting. Our licences allow it, and it is how federation grows.

## How we build

TNHC's software is built by AI under human direction. The people who direct it are responsible for what ships; that responsibility is never handed to the AI.

We are open about this. Our repositories say so, and our commits record when AI co-authored them. How we write code is defined in our engineering standard.

## Who decides

Today the founder, Zajfan, makes the final decisions.

That changes once TNHC has three regular contributors — people with accepted contributions in at least three of the last six months. From then on:

- Major changes are proposed in public as RFCs and discussed in the open before they are decided.
- Maintainers make the decisions within the areas they look after.
- This charter can only be changed through that same RFC process.

## Taking part

- **Report a problem**, including a promise we are not keeping: open an issue on the relevant repository at [github.com/The-No-Hands-company](https://github.com/The-No-Hands-company), or email info@tnhc.dev.
- **Suggest an idea:** open an issue. Once the RFC process exists, larger ideas go there.
- **Contribute:** open a pull request. Each project's README explains how to build and test it.

Everyone taking part follows our [Code of Conduct](https://github.com/The-No-Hands-company/handbook/blob/main/CODE_OF_CONDUCT.md).
```

- [ ] **Step 5: Code of Conduct and licence (downloaded verbatim)**

```bash
curl -fsSL https://www.contributor-covenant.org/version/2/1/code_of_conduct/code_of_conduct.md -o CODE_OF_CONDUCT.md
grep -n '\[INSERT CONTACT METHOD\]' CODE_OF_CONDUCT.md
sed -i 's/\[INSERT CONTACT METHOD\]/info@tnhc.dev/' CODE_OF_CONDUCT.md
curl -fsSL https://creativecommons.org/licenses/by/4.0/legalcode.txt -o LICENSE
```

Expected: the `grep` shows exactly one line before the `sed`; afterwards `grep -c 'INSERT CONTACT' CODE_OF_CONDUCT.md` prints `0`.

- [ ] **Step 6: Ask the user for the finances starting record**

Ask: "Have you received any donations or paid any TNHC expenses you want listed (domain ~$12.20/yr, anything else)?" Write `finances.md` from the answer:

```markdown
# Finances

Every donation TNHC receives and every expense it pays, in the order they happened. Donations buy nothing; see the [Charter](https://tnhc.dev/charter#money).

| Date | Type | Amount | What for |
| --- | --- | --- | --- |
```

followed by one row per item the user lists (`expense` / `donation`), or, if none, the single row `| 2026-10-06 | — | — | Record started; no donations received |`.

- [ ] **Step 7: README**

```markdown
# The No Hands Company Handbook

How The No Hands Company works, in public. Chapters:

1. [Charter](charter.md) — what TNHC is, what it promises, and how it is run. Published at https://tnhc.dev/charter.

Also here: our [Code of Conduct](CODE_OF_CONDUCT.md) and our [finances](finances.md).

Everything in this repository is licensed [CC BY 4.0](LICENSE).
```

- [ ] **Step 8: Run the check**

Run: `scripts/check-charter.sh`
Expected: `PASS`

- [ ] **Step 9: Commit and push**

```bash
git add -A && git commit -m "docs: TNHC Charter, Code of Conduct, finances (CC BY 4.0)" && git push -u origin main
gh repo view The-No-Hands-company/handbook --json licenseInfo -q .licenseInfo.spdxId
```

Expected: `CC-BY-4.0`. Record the pushed commit SHA (`git rev-parse HEAD`) — Task 5 pins to it.

---

### Task 2: Phantom — move into the organisation, licence, status page

**Files:**
- Repo transfer: `Zajfan/Phantom` → `The-No-Hands-company/Phantom`
- Modify (Nexus-Systems): `.gitmodules`, `docs/ARCHITECTURE.md`, `.github/workflows/nexus-release-gate.yml`
- Create (Phantom): `LICENSE-APACHE`, `LICENSE-MIT`, `STATUS.md`, `scripts/check-status.sh`

**Interfaces:**
- Produces: `STATUS.md` at Phantom's root; first line `# Phantom Protocol — Status`; one table with columns `Capability | Status | Proved by`, status values exactly `Done`, `Partial`, `Not done`. Task 5 renders it.

- [ ] **Step 1: Transfer the repository** (the user approved this move)

```bash
gh api -X POST repos/Zajfan/Phantom/transfer -f new_owner=The-No-Hands-company
gh repo view The-No-Hands-company/Phantom --json name,owner -q '.owner.login+"/"+.name'
```

Expected: `The-No-Hands-company/Phantom`. If the API refuses (permissions), stop and ask the user to transfer it via Settings → General → Danger Zone → Transfer.

- [ ] **Step 2: Repoint the submodule and references in Nexus-Systems**

```bash
cd /run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems
git config -f .gitmodules submodule.apps/Phantom.url git@github.com:The-No-Hands-company/Phantom.git
git submodule sync apps/Phantom
git -C apps/Phantom remote get-url origin
grep -rn 'Zajfan/Phantom' docs/ARCHITECTURE.md .github/workflows/nexus-release-gate.yml
sed -i 's#Zajfan/Phantom#The-No-Hands-company/Phantom#g' docs/ARCHITECTURE.md .github/workflows/nexus-release-gate.yml
grep -rn 'Zajfan/Phantom' --include=*.md --include=*.yml --include=*.json --include=*.toml . | grep -v node_modules | grep -v docs/superpowers
```

Expected: origin prints the org URL; the final grep prints nothing.

- [ ] **Step 3: Write the failing status check (in Phantom)**

`apps/Phantom/scripts/check-status.sh`:

```bash
#!/usr/bin/env bash
# Every test STATUS.md cites must exist; every "Not done" row must cite a test
# that is #[ignore]d, and every "Done" row a test that is not.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f STATUS.md ] || { echo "FAIL: STATUS.md missing"; exit 1; }
fail=0
while IFS='|' read -r _ cap status proof _; do
  status=$(echo "$status" | xargs | sed -E 's/^(Done|Partial|Not done).*/\1/'); test=$(echo "$proof" | grep -oE '`[a-z_]+`' | head -1 | tr -d '`')
  case "$status" in Done|Partial|"Not done") ;; *) continue ;; esac
  [ -n "$test" ] || { echo "FAIL: no test cited for:$cap"; fail=1; continue; }
  loc=$(grep -rn --include=*.rs -E "fn $test\(" crates | head -1)
  [ -n "$loc" ] || { echo "FAIL: test not found: $test"; fail=1; continue; }
  file=${loc%%:*}; line=$(echo "$loc" | cut -d: -f2)
  ignored=$(sed -n "$((line-3)),$((line-1))p" "$file" | grep -c '#\[ignore' || true)
  if [ "$status" = "Not done" ] && [ "$ignored" = 0 ]; then echo "FAIL: '$test' backs a Not done row but is not ignored"; fail=1; fi
  if [ "$status" = "Done" ] && [ "$ignored" != 0 ]; then echo "FAIL: '$test' backs a Done row but is ignored"; fail=1; fi
done < <(grep '^|' STATUS.md | tail -n +3)
[ "$fail" = 0 ] && echo PASS || exit 1
```

Run: `chmod +x apps/Phantom/scripts/check-status.sh && apps/Phantom/scripts/check-status.sh`
Expected: `FAIL: STATUS.md missing`

- [ ] **Step 4: Confirm each cited test's current state before writing claims**

```bash
cd apps/Phantom
cargo test -p phantom-crypto test_dilithium_signatures 2>&1 | grep 'test result'
cargo test -p phantom-crypto test_fhe_basic_encryption 2>&1 | grep 'test result'
cargo test -p phantom-routing test_routing_blob_construction 2>&1 | grep 'test result'
cargo test -p phantom-routing test_multi_hop_forwarding 2>&1 | grep 'test result'
cargo test -p phantom-routing test_replay_attack_detection 2>&1 | grep 'test result'
cargo test -p phantom-routing -- --ignored --list 2>&1 | grep -E 'payload_is_not_readable_on_the_wire|test_end_to_end_phantom_routing'
```

Expected: the first five report `1 passed` (or `2 passed` for the replay name, defined twice) in at least one result line; the last lists both ignored tests. If any differs, the matching row below must change to reflect the real state — never the other way round.

- [ ] **Step 5: Write `STATUS.md`**

```markdown
# Phantom Protocol — Status

What the Phantom Protocol does today, and what it does not yet do. Each row names the test that proves it. For unfinished work, it names the test that is switched off until the work lands — that test passes, unchanged, the day it is done.

**Phantom is not yet used by any live TNHC service.** Sign-in, email and the dashboard currently use ordinary HTTPS.

| Capability | Status | Proved by |
| --- | --- | --- |
| Post-quantum signatures (Dilithium-5) | Done | `test_dilithium_signatures` (phantom-crypto) |
| Fully homomorphic encryption primitive | Done | `test_fhe_basic_encryption` (phantom-crypto) |
| Encrypted routing — a relay cannot learn the route | Done | `test_routing_blob_construction` (phantom-routing) |
| Multi-hop forwarding | Done | `test_multi_hop_forwarding` (phantom-routing) |
| Replay protection | Partial — repeated packet ids are rejected, but the nullifier is derived from the packet id, so a resent packet with a new id is not caught | `test_replay_attack_detection` (phantom-routing) |
| Message contents encrypted on the wire | Not done | `payload_is_not_readable_on_the_wire` (phantom-routing) |
| Membership proof (zero-knowledge) | Not done — the circuit is incomplete, and the current proof generator is a hash, not zero-knowledge | `test_end_to_end_phantom_routing` (phantom-routing) |

This page is published at https://tnhc.dev/phantom. Licensed MIT OR Apache-2.0, like the code.
```

Each status cell starts with exactly `Done`, `Partial` or `Not done`; the checker reads that leading word.

- [ ] **Step 6: Run the check**

Run: `scripts/check-status.sh`
Expected: `PASS`

- [ ] **Step 7: Licence files** (Cargo.toml already declares `MIT OR Apache-2.0`)

```bash
git log --format='%an <%ae>' | sort -u   # author check (Global Constraints)
curl -fsSL https://www.apache.org/licenses/LICENSE-2.0.txt -o LICENSE-APACHE
cat > LICENSE-MIT <<'EOF'
MIT License

Copyright (c) The No Hands Company contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF
grep -rn 'All rights reserved\|™' --include=*.md --include=*.toml . | grep -v target | grep -v '/rust_out/'
```

Expected: last grep prints nothing (fix any TNHC-authored hit).

- [ ] **Step 8: Commit and push Phantom, then the submodule pointer**

```bash
git add STATUS.md scripts/check-status.sh LICENSE-APACHE LICENSE-MIT
git commit -m "docs: add status page and licence files (MIT OR Apache-2.0)" && git push origin HEAD:main
cd ../.. && git add .gitmodules apps/Phantom docs/ARCHITECTURE.md .github/workflows/nexus-release-gate.yml
git commit -m "chore: Phantom moved to The-No-Hands-company; repoint submodule" && git push origin main
```

---

### Task 3: zajfan.dev — move into the organisation, licence, footer

**Files (repo moved to `The-No-Hands-company/zajfan.dev`, local `/run/media/zajferx/Data/dev/The-No-hands-Company/zajfan.dev`):**
- Modify: `src/components/Footer.astro`
- Create: `LICENSE`, `LICENSING.md`

- [ ] **Step 1: Transfer and repoint**

```bash
gh api -X POST repos/Zajfan/zajfan.dev/transfer -f new_owner=The-No-Hands-company
cd /run/media/zajferx/Data/dev/The-No-hands-Company/zajfan.dev
git remote set-url origin git@github.com:The-No-Hands-company/zajfan.dev.git && git fetch origin
```

- [ ] **Step 2: Ask the user to reconnect the Cloudflare build** (dashboard step — Cloudflare's Git integration is bound to the old owner)

Give these instructions, then wait for "done": Cloudflare dashboard → search (Ctrl+K) "Workers & Pages" → open the Worker serving zajfan.tnhc.dev → **Settings → Build** (or "Builds") → Git repository → **Disconnect**, then **Connect** → choose GitHub → grant access to the **The-No-Hands-company** organisation if asked → select `zajfan.dev`, branch `main`. Leave build/deploy commands unchanged (`npx wrangler deploy` per README).

- [ ] **Step 3: Footer** — replace the bottom-row left `div` (`© {currentYear} Zajfan. Built with Astro & Tailwind.`) with:

```astro
      <div>
        Zajfan · Part of <a href="https://tnhc.dev" class="hover:text-accent">The No Hands Company</a> ·
        <a href="https://tnhc.dev/charter" class="hover:text-accent">Charter</a>
      </div>
```

and delete the now-unused `const currentYear = new Date().getFullYear();` frontmatter line. The Resources list keeps "GitHub (Personal)" unchanged.

- [ ] **Step 4: Licences** (author check first)

```bash
git log --format='%an <%ae>' | sort -u
curl -fsSL https://www.gnu.org/licenses/agpl-3.0.txt -o LICENSE
cat > LICENSING.md <<'EOF'
# Licensing

- Site code: AGPL-3.0-or-later (see `LICENSE`).
- Written content (posts, devlog, project pages): CC BY 4.0 — https://creativecommons.org/licenses/by/4.0/

See the TNHC Charter: https://tnhc.dev/charter
EOF
grep -rn 'All rights reserved\|™' src | head
```

Expected: last grep prints nothing.

- [ ] **Step 5: Build, commit, push, verify live**

```bash
bun run build && grep -c 'tnhc.dev/charter' dist/index.html
git add -A src LICENSE LICENSING.md && git commit -m "feat: licence (AGPL code, CC BY content) and Charter link" && git push origin main
```

Expected: count ≥ 1. After the Cloudflare build finishes: `curl -s https://zajfan.tnhc.dev/ | grep -c 'tnhc.dev/charter'` ≥ 1.

---

### Task 4: Nexus-Systems — per-directory licensing with a checker

**Files:**
- Create: `scripts/check-licenses.sh`, `LICENSE`, `LICENSING.md`, `packages/<each>/LICENSE`
- Modify: `packages/<each>/package.json` (`"license": "Apache-2.0"`), `packages/<each>/Cargo.toml` (`license = "Apache-2.0"`), `check.sh`

- [ ] **Step 1: Write the failing checker**

`scripts/check-licenses.sh`:

```bash
#!/usr/bin/env bash
# The monorepo is AGPL at the root and Apache-2.0 under packages/. Each package
# must say so in a LICENSE file and in its manifest, so tools agree with humans.
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0
grep -q 'GNU AFFERO GENERAL PUBLIC LICENSE' LICENSE 2>/dev/null || { echo "FAIL: root LICENSE is not AGPL-3.0"; fail=1; }
[ -f LICENSING.md ] || { echo "FAIL: LICENSING.md missing"; fail=1; }
for d in packages/*/; do
  p=${d%/}
  grep -q 'Apache License' "$p/LICENSE" 2>/dev/null || { echo "FAIL: $p/LICENSE is not Apache-2.0"; fail=1; }
  if [ -f "$p/package.json" ]; then
    [ "$(jq -r '.license // ""' "$p/package.json")" = "Apache-2.0" ] || { echo "FAIL: $p/package.json license"; fail=1; }
  fi
  if [ -f "$p/Cargo.toml" ]; then
    grep -qE '^license *= *"Apache-2.0"' "$p/Cargo.toml" || { echo "FAIL: $p/Cargo.toml license"; fail=1; }
  fi
done
[ "$fail" = 0 ] && echo PASS || exit 1
```

Run: `chmod +x scripts/check-licenses.sh && scripts/check-licenses.sh`
Expected: `FAIL: root LICENSE is not AGPL-3.0` plus one FAIL per package.

- [ ] **Step 2: Author check and third-party inventory**

```bash
git log --format='%an <%ae>' | sort -u
find . -path ./node_modules -prune -o \( -iname 'LICENSE*' -o -iname 'COPYING*' \) -print | grep -v node_modules | grep -v '/target/' | sort
```

Record the inventory in `LICENSING.md` (Step 3). Existing files stay untouched.

- [ ] **Step 3: Root licence and LICENSING.md**

```bash
curl -fsSL https://www.gnu.org/licenses/agpl-3.0.txt -o LICENSE
```

`LICENSING.md`:

```markdown
# Licensing

This repository is licensed per directory, following the [TNHC Charter](https://tnhc.dev/charter#ownership-and-licences):

| Path | Licence |
| --- | --- |
| Everything not listed below | AGPL-3.0-or-later (`LICENSE`) |
| `packages/*` | Apache-2.0 (each package's own `LICENSE`) |
| `docs/` | CC BY 4.0 |
| Submodules (`apps/Phantom`, …) | Their own repository's licence |

Third-party code keeps its original licence. Files found at the time of licensing:

<paste the Step 2 inventory here as a bullet list, one path per line, excluding the files this task creates>
```

Replace the angle-bracket line with the actual inventory before committing (it is data from Step 2, not a placeholder to leave).

- [ ] **Step 4: Package licences and manifests**

```bash
for p in packages/*/; do curl -fsSL https://www.apache.org/licenses/LICENSE-2.0.txt -o "${p}LICENSE"; done
for f in packages/*/package.json; do jq '.license = "Apache-2.0"' "$f" > "$f.tmp" && mv "$f.tmp" "$f"; done
for f in packages/*/Cargo.toml; do
  if grep -qE '^license *=' "$f"; then sed -i -E 's/^license *=.*/license = "Apache-2.0"/' "$f"
  else sed -i -E '0,/^\[package\]/s//[package]\nlicense = "Apache-2.0"/' "$f"; fi
done
```

(Download once to a temp file and copy if the network is slow; the text must be the official one.)

- [ ] **Step 5: Run the checker**

Run: `scripts/check-licenses.sh`
Expected: `PASS`

- [ ] **Step 6: Gate it** — add to the root `check.sh`, before the per-app loop:

```bash
bash scripts/check-licenses.sh
```

Run: `bash scripts/check-licenses.sh && git diff --stat | tail -1`

- [ ] **Step 7: Remove contradicting notices**

```bash
grep -rIn --exclude-dir=node_modules --exclude-dir=target --exclude-dir=.git -E 'All rights reserved|™' . | grep -v -i 'third.party\|vendor' | head -50
```

Fix each TNHC-authored hit (delete the `©… All rights reserved` line, or replace with `Licensed under the TNHC Charter: https://tnhc.dev/charter`). Leave third-party files alone.

- [ ] **Step 8: Commit and push**

```bash
git add LICENSE LICENSING.md scripts/check-licenses.sh check.sh packages
git commit -m "chore: license the monorepo (AGPL root, Apache-2.0 packages) with a checker" && git push origin main
gh repo view The-No-Hands-company/Nexus-Systems --json licenseInfo -q .licenseInfo.spdxId
```

Expected: `AGPL-3.0`.

---

### Task 5: tnhc.dev — Charter and Phantom pages, footers, licence

**Files (repo `tnhc.dev`, local `/run/media/zajferx/Data/dev/The-No-hands-Company/tnhc.dev`):**
- Create: `scripts/build-markdown-page.py`, `tests/test_build_markdown_page.py`, `frontend/src/data/charter.js`, `frontend/src/data/phantomStatus.js`, `frontend/src/components/site/MarkdownPage.jsx`, `frontend/src/components/site/SiteFooterLinks.jsx`, `frontend/src/pages/Charter.jsx`, `frontend/src/pages/PhantomStatus.jsx`, `LICENSE`
- Modify: `frontend/src/App.js` (routes), `frontend/src/pages/Blog.jsx`, `frontend/src/pages/Apps.jsx`, `frontend/src/components/site/Waitlist.jsx` (footers), `scripts/webmaster-sync.sh`, `scripts/build-sitemap.py`

**Interfaces:**
- Consumes: handbook `charter.md` (Task 1), Phantom `STATUS.md` (Task 2).
- Produces: `build-markdown-page.py --repo <git dir> --file <path in repo> --out <js file> --export <NAME> [--ref <git ref>]` writing `export const <NAME> = { html: "<string>", source: "<github url>", commit: "<40-hex sha>" };`

- [ ] **Step 1: Failing generator test**

`tests/test_build_markdown_page.py`:

```python
import json, re, subprocess, sys, pathlib

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts/build-markdown-page.py"

def make_repo(tmp_path, text):
    repo = tmp_path / "src"; repo.mkdir()
    (repo / "doc.md").write_text(text)
    for cmd in (["init", "-q", "-b", "main"], ["add", "doc.md"],
                ["-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "x"]):
        subprocess.run(["git", "-C", str(repo), *cmd], check=True)
    sha = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    return repo, sha

def run(repo, out, *extra):
    return subprocess.run([sys.executable, str(SCRIPT), "--repo", str(repo), "--file", "doc.md",
                           "--out", str(out), "--export", "DOC", "--source", "https://example.test/doc", *extra],
                          capture_output=True, text=True)

def parse(out):
    body = out.read_text()
    m = re.search(r"export const DOC = (\{.*\});\s*$", body, re.S)
    assert m, body
    return json.loads(m.group(1))

def test_renders_committed_markdown_with_commit(tmp_path):
    repo, sha = make_repo(tmp_path, "# Title\n\n| a | b |\n| - | - |\n| 1 | 2 |\n")
    out = tmp_path / "doc.js"
    r = run(repo, out)
    assert r.returncode == 0, r.stderr
    data = parse(out)
    assert data["commit"] == sha
    assert "<h1" in data["html"] and "<table>" in data["html"]
    assert data["source"] == "https://example.test/doc"
    assert out.read_text().startswith("// GENERATED")

def test_uses_committed_text_not_working_tree(tmp_path):
    repo, _ = make_repo(tmp_path, "# Committed\n")
    (repo / "doc.md").write_text("# Uncommitted edit\n")
    out = tmp_path / "doc.js"
    assert run(repo, out).returncode == 0
    assert "Committed" in parse(out)["html"] and "Uncommitted" not in parse(out)["html"]

def test_fails_loudly_when_file_missing(tmp_path):
    repo, _ = make_repo(tmp_path, "# x\n")
    out = tmp_path / "doc.js"
    r = subprocess.run([sys.executable, str(SCRIPT), "--repo", str(repo), "--file", "nope.md",
                        "--out", str(out), "--export", "DOC", "--source", "s"], capture_output=True, text=True)
    assert r.returncode != 0 and not out.exists()

def test_headings_get_ids_for_anchor_links(tmp_path):
    repo, _ = make_repo(tmp_path, "## Ownership and licences\n")
    out = tmp_path / "doc.js"
    assert run(repo, out).returncode == 0
    assert 'id="ownership-and-licences"' in parse(out)["html"]
```

Run: `python3 -m pytest tests/test_build_markdown_page.py -q`
Expected: 4 failed (script missing).

- [ ] **Step 2: Generator**

`scripts/build-markdown-page.py`:

```python
#!/usr/bin/env python3
"""Render one Markdown file from another repository into a JS data module.

The site never hand-copies documents it does not own (see WEBMASTER.md): the
Charter lives in the handbook repo and the Phantom status beside Phantom's
code. This reads the file as *committed* at --ref, so the page always matches a
real, reviewable commit, and records that commit in the output.
"""
import argparse, json, pathlib, subprocess, sys
import markdown

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True)
    ap.add_argument("--file", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--export", required=True)
    ap.add_argument("--source", required=True)
    ap.add_argument("--ref", default="HEAD")
    a = ap.parse_args()

    def git(*args):
        return subprocess.run(["git", "-C", a.repo, *args], capture_output=True, text=True)

    sha = git("rev-parse", a.ref)
    if sha.returncode != 0:
        print(f"FAIL: cannot resolve {a.ref} in {a.repo}", file=sys.stderr); return 1
    sha = sha.stdout.strip()
    text = git("show", f"{sha}:{a.file}")
    if text.returncode != 0:
        print(f"FAIL: {a.file} not found at {sha}", file=sys.stderr); return 1

    html = markdown.markdown(text.stdout, extensions=["tables", "toc"],
                             extension_configs={"toc": {"permalink": False}})
    data = {"html": html, "source": a.source, "commit": sha}
    pathlib.Path(a.out).write_text(
        "// GENERATED by scripts/build-markdown-page.py — do not hand-edit.\n"
        f"// Source: {a.source} at {sha}\n"
        f"export const {a.export} = {json.dumps(data, ensure_ascii=False)};\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

The `toc` extension's default slugify lowercases and hyphenates, producing `ownership-and-licences` (the Charter's `#money` / `#ownership-and-licences` links rely on it).

- [ ] **Step 3: Tests pass**

Run: `python3 -m pytest tests/test_build_markdown_page.py -q`
Expected: `4 passed`

- [ ] **Step 4: Wire into webmaster-sync** — in `scripts/webmaster-sync.sh`, add both files to `GEN=(…)` and, after the `build-apis.py` line:

```bash
HANDBOOK="${HANDBOOK:-../handbook}"
[ -d "$HANDBOOK/.git" ] || git clone --quiet https://github.com/The-No-Hands-company/handbook.git "$HANDBOOK"
git -C "$HANDBOOK" pull --quiet --ff-only origin main || echo "WARN: could not update handbook; using local copy"
python3 scripts/build-markdown-page.py --repo "$HANDBOOK" --file charter.md \
    --out frontend/src/data/charter.js --export CHARTER \
    --source https://github.com/The-No-Hands-company/handbook/blob/main/charter.md \
    || { echo "FAIL: charter generation" >&2; exit 1; }
python3 scripts/build-markdown-page.py --repo "$REPO/apps/Phantom" --file STATUS.md \
    --out frontend/src/data/phantomStatus.js --export PHANTOM_STATUS \
    --source https://github.com/The-No-Hands-company/Phantom/blob/main/STATUS.md \
    || { echo "FAIL: Phantom status generation" >&2; exit 1; }
```

(`../handbook` resolves to `/run/media/zajferx/Data/dev/The-No-hands-Company/handbook`, beside tnhc.dev.) Run `bash scripts/webmaster-sync.sh` and confirm both data files exist and the charter file's `commit` equals Task 1's SHA.

- [ ] **Step 5: Shared page component**

`frontend/src/components/site/MarkdownPage.jsx`:

```jsx
import Header from "@/components/site/Header";
import SiteFooterLinks from "@/components/site/SiteFooterLinks";

// Renders a generated document (see scripts/build-markdown-page.py). The HTML
// comes from our own committed Markdown, never from user input.
export default function MarkdownPage({ doc, testId }) {
  return (
    <main className="min-h-screen bg-void text-white">
      <Header />
      <article
        className="charter-prose mx-auto max-w-3xl px-6 pb-24 pt-32 md:px-12"
        data-testid={testId}
        dangerouslySetInnerHTML={{ __html: doc.html }}
      />
      <p className="mx-auto max-w-3xl px-6 pb-16 font-mono text-[11px] uppercase tracking-[0.25em] text-white/40 md:px-12">
        <a href={doc.source} target="_blank" rel="noopener noreferrer" className="hover:text-acid">
          Source · {doc.commit.slice(0, 7)}
        </a>
      </p>
      <footer className="border-t border-white/10">
        <div className="mx-auto max-w-[1600px] px-6 py-8 md:px-12"><SiteFooterLinks /></div>
      </footer>
    </main>
  );
}
```

Add to `frontend/src/index.css` (Tailwind is configured; no typography plugin is installed, so style the prose directly):

```css
.charter-prose h1 { font-size: 2.5rem; line-height: 1.1; font-weight: 700; margin-bottom: 1.5rem; }
.charter-prose h2 { font-size: 1.5rem; font-weight: 700; margin: 3rem 0 1rem; color: #ccff00; }
.charter-prose h3 { font-size: 1.125rem; font-weight: 700; margin: 2rem 0 0.75rem; }
.charter-prose p, .charter-prose li { color: rgb(255 255 255 / 0.8); line-height: 1.7; }
.charter-prose p { margin-bottom: 1rem; }
.charter-prose ul { list-style: disc; padding-left: 1.25rem; margin-bottom: 1rem; }
.charter-prose a { color: #ccff00; text-decoration: underline; text-underline-offset: 3px; }
.charter-prose table { width: 100%; border-collapse: collapse; margin: 1.5rem 0; font-size: 0.95rem; }
.charter-prose th, .charter-prose td { border: 1px solid rgb(255 255 255 / 0.12); padding: 0.6rem 0.75rem; text-align: left; vertical-align: top; }
.charter-prose code { font-family: "JetBrains Mono", monospace; font-size: 0.85em; }
```

- [ ] **Step 6: Footer links component and pages**

`frontend/src/components/site/SiteFooterLinks.jsx`:

```jsx
import { Link } from "react-router-dom";

export default function SiteFooterLinks() {
  return (
    <span className="font-mono text-[11px] uppercase tracking-[0.25em] text-white/40">
      The No Hands Company ·{" "}
      <Link to="/charter" className="transition-colors hover:text-acid" data-testid="footer-charter">Charter</Link>
      {" · "}
      <a href="https://zajfan.tnhc.dev" target="_blank" rel="noopener noreferrer" className="transition-colors hover:text-acid">
        Founder: Zajfan
      </a>
    </span>
  );
}
```

`frontend/src/pages/Charter.jsx`:

```jsx
import MarkdownPage from "@/components/site/MarkdownPage";
import { CHARTER } from "@/data/charter";

export default function Charter() {
  return <MarkdownPage doc={CHARTER} testId="charter-page" />;
}
```

`frontend/src/pages/PhantomStatus.jsx`:

```jsx
import MarkdownPage from "@/components/site/MarkdownPage";
import { PHANTOM_STATUS } from "@/data/phantomStatus";

export default function PhantomStatus() {
  return <MarkdownPage doc={PHANTOM_STATUS} testId="phantom-status-page" />;
}
```

In `frontend/src/App.js`, import both and add beside the existing routes:

```jsx
          <Route path="/charter" element={<Charter />} />
          <Route path="/phantom" element={<PhantomStatus />} />
```

- [ ] **Step 7: Replace the footer copyright spans** — in `pages/Blog.jsx`, `pages/Apps.jsx` and `components/site/Waitlist.jsx`, replace the `<span …>© {new Date().getFullYear()} The No Hands Company · <a …>Founder: Zajfan</a></span>` element with `<SiteFooterLinks />` (and import it). Then:

```bash
grep -rn '©\|All rights reserved' frontend/src | grep -v '/data/'
```

Expected: nothing.

- [ ] **Step 8: Sitemap** — in `scripts/build-sitemap.py`, add `"/charter"` and `"/phantom"` to its static route list (find it with `grep -n '"/apps"' scripts/build-sitemap.py`), then run `python3 scripts/build-sitemap.py`.

- [ ] **Step 9: Licence**

```bash
git log --format='%an <%ae>' | sort -u
curl -fsSL https://www.gnu.org/licenses/agpl-3.0.txt -o LICENSE
```

- [ ] **Step 10: Build and verify locally**

```bash
cd frontend && npx craco build 2>&1 | tail -3 && cd ..
grep -l 'The No Hands Company Charter' frontend/build/static/js/*.js | head -1
```

Expected: build succeeds; one bundle contains the Charter title.

- [ ] **Step 11: Commit, push, verify live** (Cloudflare Pages deploys on push)

```bash
git add -A scripts tests frontend/src LICENSE frontend/public/sitemap.xml
git commit -m "feat: Charter and Phantom status pages generated from their sources; footer Charter link" && git push origin main
```

After the Pages build: `curl -s -o /dev/null -w '%{http_code}' https://tnhc.dev/charter` → `200`; compare the live `index.html` bundle hash to `frontend/build/index.html` (WEBMASTER.md "pushing is not deploying").

- [ ] **Step 12: Ask about making the repo public**

`tnhc.dev` is the only **private** TNHC repo, which contradicts "everything we make is open source". Run a secrets scan first and show the user the result:

```bash
git log -p --all | grep -nE '(api[_-]?key|secret|token|password)\s*[:=]\s*["'"'"'][^"'"'"' ]{12,}' | head -20
ls -a | grep -E '^\.env'
```

Then ask: "tnhc.dev's repository is private. Make it public? (Scan result: …)". Only on an explicit yes: `gh repo edit The-No-Hands-company/tnhc.dev --visibility public --accept-visibility-change-consequences`.

---

### Task 6: Dashboard footer link

**Files (Nexus-Systems):**
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.tsx:201-213`
- Test: `apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx`

- [ ] **Step 1: Failing test** — append inside `describe("Home", …)` in `Home.test.tsx`:

```tsx
  it("links the TNHC Charter from the footer", async () => {
    stubFetch(true);
    render(<MemoryRouter><Home /></MemoryRouter>);
    const footer = await screen.findByRole("contentinfo");
    const link = within(footer).getByRole("link", { name: "Charter" });
    expect(link.getAttribute("href")).toBe("https://tnhc.dev/charter");
  });
```

and add `within` to the `@testing-library/react` import.

- [ ] **Step 2: Run it to see it fail**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/pages/Home.test.tsx`
Expected: FAIL — unable to find a link named "Charter".

- [ ] **Step 3: Add the link** — in `Home.tsx`, after the `Changelog` anchor:

```tsx
          <a href="https://tnhc.dev/charter" className="hover:text-zinc-100">Charter</a>
```

- [ ] **Step 4: Run the test file and the full dashboard suite**

Run: `npx vitest run`
Expected: all pass.

- [ ] **Step 5: Commit, then deploy** (the build here is the production deploy)

```bash
git add src/pages/Home.tsx src/pages/Home.test.tsx
git commit -m "feat(dashboard): link the TNHC Charter from the footer" && git push origin main
bun run build
curl -s https://app.tnhc.dev/ -o /dev/null -w '%{http_code}\n'
```

Expected: build succeeds; the footer link is visible after sign-in at app.tnhc.dev (compare hashed asset names in the live `index.html` with `dist/index.html`).

---

### Task 7: tnhc-community — licence and Charter link

**Files (repo `The-No-Hands-company/tnhc-community`, local `/run/media/zajferx/Data/dev/The-No-hands-Company/projects/TNHC-Community`):**
- Create: `LICENSE`
- Modify: `app/src/main/java/com/tnhc/community/ui/AccountScreen.kt`

The app has no About screen; the Account screen is the closest equivalent and holds the link.

- [ ] **Step 1: Find the unit/UI test style**

```bash
ls app/src/test app/src/androidTest 2>/dev/null; grep -rln 'AccountScreen\|account-sign-in' app/src/test app/src/androidTest 2>/dev/null
```

If an AccountScreen Compose test exists, add a case asserting `onNodeWithTag("account-charter").assertExists()`, run it, see it fail. If none exists, skip to Step 2 (no new test harness for one link).

- [ ] **Step 2: Add the link** at the end of the AccountScreen `Column`, after the signed-in/signed-out branches:

```kotlin
        val uriHandler = LocalUriHandler.current
        TextButton(
            onClick = { uriHandler.openUri("https://tnhc.dev/charter") },
            modifier = Modifier.testTag("account-charter"),
        ) { Text("The TNHC Charter") }
```

with `import androidx.compose.ui.platform.LocalUriHandler` (and `TextButton` / `testTag` imports if not already present).

- [ ] **Step 3: Build and test**

Run: `./gradlew testDebugUnitTest assembleDebug`
Expected: BUILD SUCCESSFUL.

- [ ] **Step 4: Licence, commit, push**

```bash
git log --format='%an <%ae>' | sort -u
curl -fsSL https://www.gnu.org/licenses/agpl-3.0.txt -o LICENSE
git add LICENSE app/src/main/java/com/tnhc/community/ui/AccountScreen.kt
git commit -m "feat: AGPL licence and a Charter link on the Account screen" && git push origin main
```

The link reaches users with the next store release; publishing a release is out of scope.

---

### Task 8: Licence the remaining public organisation repos

**Proposed classification** (confirm with the user in Step 1 before applying):

| Licence | Repos |
| --- | --- |
| AGPL-3.0-or-later | Nexus-Modeling, Nexus-Cloud, Nexus-Hosting, Nexus-Chat, Nexus-Deploy, Nexus-Vault, Nexus-Network, Nexus-AI, Nexus-Computer, Nexus-Forge, devvault, versatodo, VersaAI, api-key-registry, https-github.com-The-No-Hands-company.github.io |
| Apache-2.0 | Nexuslang (language runtime linked into users' programs), Nit |
| CC BY 4.0 | .github, The-No-hands-Company |

- [ ] **Step 1: Confirm the table with the user** — show it and ask: "OK to apply these licences? Change any row?" Apply the answer.

- [ ] **Step 2: For each repo**, in `~/tnhc-charter-work`:

```bash
r=<repo name from the table>; lic=<AGPL|APACHE|CCBY>
gh repo clone "The-No-Hands-company/$r" "$r" -- --depth=200 && cd "$r"
git log --format='%an <%ae>' | sort -u           # stop and ask if anyone unexpected
ls LICENSE* COPYING* 2>/dev/null                 # if one already exists, skip this repo and report it
case $lic in
  AGPL)   curl -fsSL https://www.gnu.org/licenses/agpl-3.0.txt -o LICENSE ;;
  APACHE) curl -fsSL https://www.apache.org/licenses/LICENSE-2.0.txt -o LICENSE ;;
  CCBY)   curl -fsSL https://creativecommons.org/licenses/by/4.0/legalcode.txt -o LICENSE ;;
esac
grep -rIln --exclude-dir=.git --exclude-dir=node_modules -E 'All rights reserved|™' . | head
```

Fix TNHC-authored hits from the last grep, then:

```bash
git add -A && git commit -m "chore: add licence per the TNHC Charter" && git push origin HEAD
cd ..
```

(The commit message ends with the session's Co-Authored-By line, as everywhere.)

- [ ] **Step 3: Organisation profile** — in the `.github` clone, edit `profile/README.md`: directly under its first heading insert

```markdown
**Free, open and yours.** We offer for free the kind of software and services others charge for. Everything is open source, nothing is sold or paywalled, nothing locks you in, and we collect no data of any kind. Read the [TNHC Charter](https://tnhc.dev/charter).
```

Commit `docs: link the Charter from the organisation profile` and push.

- [ ] **Step 4: Verify**

```bash
gh repo list The-No-Hands-company --limit 100 --visibility public --json name,licenseInfo -q '.[] | "\(.name) \(.licenseInfo.spdxId // "NONE")"'
```

Expected: no `NONE` rows. (`.github` and `The-No-hands-Company` may show `NOASSERTION` for CC BY 4.0 — acceptable; confirm the file is the CC text.)

---

### Task 9: Local org documents and the Zajfan Standard

These files exist only in the local org folder (not published in any repo), so they are fixed in place and **not committed** (see Global Constraints). They become handbook chapters later (Brand, Engineering).

**Files (in `/run/media/zajferx/Data/dev/The-No-hands-Company`):**
- Modify: `branding/style-guide/README.md`, `branding/logos/README.md`, `docs/branding/README.md`, `docs/mission-statement/README.md`, `zajfan-standard/LICENSE`, `zajfan-standard/README.md` (if it mentions "internal")

- [ ] **Step 1: Find every hit**

```bash
cd /run/media/zajferx/Data/dev/The-No-hands-Company
grep -rIn -E 'All rights reserved|™|INTERNAL ARCHITECTURAL' branding docs zajfan-standard .github --exclude-dir=node_modules
```

- [ ] **Step 2: Fix them**
  - Replace each `© <year> The No Hands Company. All rights reserved.` footer with `Licensed CC BY 4.0 — see the TNHC Charter: https://tnhc.dev/charter`.
  - In `branding/style-guide/README.md`, change `- Legal name: The No Hands Company™` to `- Name: The No Hands Company (see the Charter's name-and-logo policy)`.
  - Replace `zajfan-standard/LICENSE` with the CC BY 4.0 legal text: `curl -fsSL https://creativecommons.org/licenses/by/4.0/legalcode.txt -o zajfan-standard/LICENSE`, and remove "internal" wording from its README.

- [ ] **Step 3: Verify**

Run the Step 1 grep again. Expected: no output.

---

### Task 10: Final verification

- [ ] **Step 1: Every promise-bearing URL answers**

```bash
for u in https://tnhc.dev/charter https://tnhc.dev/phantom https://zajfan.tnhc.dev/ \
         https://github.com/The-No-Hands-company/handbook https://github.com/The-No-Hands-company/Phantom; do
  echo "$(curl -s -o /dev/null -w '%{http_code}' -L "$u") $u"; done
```

Expected: all `200`.

- [ ] **Step 2: Live pages carry the committed text**

```bash
curl -s https://tnhc.dev/charter | grep -o 'static/js/main\.[a-f0-9]*\.js' | head -1
```

Fetch that bundle and confirm it contains both `The No Hands Company Charter` and the handbook SHA from Task 1.

- [ ] **Step 3: Checkers green**

```bash
~/tnhc-charter-work/handbook/scripts/check-charter.sh
/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/apps/Phantom/scripts/check-status.sh
/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/scripts/check-licenses.sh
```

Expected: three `PASS`.

- [ ] **Step 4: Licence sweep** — rerun Task 8 Step 4. Expected: no `NONE`.

- [ ] **Step 5: Report** to the user: each surface with its Charter link, the licence of every repo, anything skipped (existing licences, unexpected authors, the tnhc.dev visibility answer), and the reminder that the Android link ships with the next app release.
