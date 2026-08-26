#!/bin/bash
# Nexus Systems — first command after cloning, and the one that fixes a working
# copy whose submodules have fallen out of step.
#
# Usage:
#   scripts/bootstrap.sh            # register and update every submodule
#   scripts/bootstrap.sh --verify   # check only, change nothing (used by CI)
#   NEXUS_SKIP_SUBMODULES=dhts/GameDevelopmentToolset scripts/bootstrap.sh
#
# What this exists for. `.gitmodules` declares eleven submodules. In this
# working copy only five of them were registered in `.git/config`, which means
# `git submodule update` silently skipped the other six — including
# apps/Nexus-Hosting, which runs in production. Nothing had drifted and a fresh
# clone is unaffected (a clone populates all eleven gitlinks and `init`
# registers all eleven), so this is narrower than it first looked: it is one
# working copy out of step, and no command in the repository put it back.
set -uo pipefail
cd "$(dirname "$0")/.."

VERIFY_ONLY=0
[ "${1:-}" = "--verify" ] && VERIFY_ONLY=1

G="\033[32m" Y="\033[33m" Rd="\033[31m" R="\033[0m"
log()  { echo -e "${G}[bootstrap]${R} $*"; }
warn() { echo -e "${Y}[bootstrap]${R} $*"; }
fail() { echo -e "${Rd}[bootstrap]${R} $*"; }

problems=0

# Everything below asks git questions. Outside a work tree those all fail in the
# same way — "not a git repository" — and the script then reports every
# submodule as missing from the index, which is alarming and wrong. Say what is
# actually the matter instead.
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  fail "not inside a git work tree — run this from a clone, not from an exported archive"
  exit 2
fi

# ── 1. .gitmodules and the index must agree ────────────────────────────────
#
# A submodule declared in .gitmodules with no gitlink in the index — or a
# gitlink with no declaration — is a repository that cannot be cloned correctly
# by anyone. Cheap to check and impossible to notice by hand.
declared=$(git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | awk '{print $2}' | sort)
indexed=$(git ls-files -s | awk '$1==160000 {print $4}' | sort)

missing_gitlink=$(comm -23 <(echo "$declared") <(echo "$indexed"))
missing_declaration=$(comm -13 <(echo "$declared") <(echo "$indexed"))

if [ -n "$missing_gitlink" ]; then
  fail "declared in .gitmodules but absent from the index:"
  echo "$missing_gitlink" | sed 's/^/    /'
  problems=1
fi
if [ -n "$missing_declaration" ]; then
  fail "present in the index but not declared in .gitmodules:"
  echo "$missing_declaration" | sed 's/^/    /'
  problems=1
fi
[ $problems -eq 0 ] && log "$(echo "$declared" | wc -l) submodules declared, all present in the index"

# ── 2. Registration and checkout ───────────────────────────────────────────
unregistered=""
while read -r path; do
  [ -z "$path" ] && continue
  git config --get "submodule.$path.url" >/dev/null 2>&1 || unregistered="$unregistered $path"
done <<< "$declared"

if [ -n "$unregistered" ]; then
  if [ $VERIFY_ONLY -eq 1 ]; then
    warn "not registered in .git/config —\`git submodule update\` will skip these:"
    for p in $unregistered; do echo "    $p"; done
    warn "run scripts/bootstrap.sh to fix"
  else
    log "registering:$unregistered"
  fi
fi

if [ $VERIFY_ONLY -eq 1 ]; then
  # Report drift without touching anything.
  git ls-files -s | awk '$1==160000 {print $2, $4}' | while read -r sha path; do
    [ -e "$path/.git" ] || continue
    disk=$(git -C "$path" rev-parse HEAD 2>/dev/null)
    [ "$sha" = "$disk" ] || warn "$path is not at the recorded commit (${sha:0:10} vs ${disk:0:10})"
  done
  [ $problems -eq 0 ] && log "verify passed" || fail "verify failed"
  exit $problems
fi

# dhts/GameDevelopmentToolset is ~408 MB and almost nothing needs it, so it can
# be skipped without skipping everything else.
skip="${NEXUS_SKIP_SUBMODULES:-}"
while read -r path; do
  [ -z "$path" ] && continue
  case " $skip " in *" $path "*) warn "skipping $path (NEXUS_SKIP_SUBMODULES)"; continue ;; esac
  if ! git submodule update --init --recursive -- "$path"; then
    fail "could not initialise $path"
    problems=1
  fi
done <<< "$declared"

if [ $problems -eq 0 ]; then
  log "all submodules initialised and at their recorded commits"
else
  fail "some submodules could not be initialised — see above"
fi
exit $problems
