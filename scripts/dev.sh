#!/usr/bin/env bash
# dev.sh — single-module dev mode for Nexus Systems
#
# Starts the docker infra + Nexus-Auth, then runs ONE module in the
# foreground. Ctrl+C stops the module; auth and infra keep running.
# Run 'scripts/dev.sh stop' to also bring down auth.
#
# Usage (from repo root):
#   scripts/dev.sh <ModuleName>   e.g. scripts/dev.sh Nexus-Dashboard
#   scripts/dev.sh auth           start infra + auth only
#   scripts/dev.sh infra          start docker infra only
#   scripts/dev.sh stop           stop auth background process
#   scripts/dev.sh down           stop auth + docker infra (full teardown)
#   scripts/dev.sh status         show what's running
#   scripts/dev.sh list           list available modules
#   scripts/dev.sh logs [name]    tail logs (default: nexus-auth)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATE_DIR="$ROOT_DIR/.local/dev"
PID_FILE="$STATE_DIR/nexus-auth.pid"
LOG_FILE="$STATE_DIR/nexus-auth.log"
AUTH_DIR="$ROOT_DIR/apps/Nexus-Auth"

mkdir -p "$STATE_DIR"

# ── colours (only when connected to a terminal) ───────────────────────────
if [[ -t 1 ]]; then
  G="\033[32m" Y="\033[33m" R="\033[31m" B="\033[34m" Z="\033[0m"
else
  G="" Y="" R="" B="" Z=""
fi

ok()   { echo -e "${G}[dev]${Z} $*"; }
warn() { echo -e "${Y}[dev]${Z} $*"; }
err()  { echo -e "${R}[dev]${Z} $*" >&2; }
info() { echo -e "${B}[dev]${Z} $*"; }

# ── guards ────────────────────────────────────────────────────────────────

need_bun() {
  if ! command -v bun &>/dev/null; then
    err "bun is not installed. Install it: curl -fsSL https://bun.sh/install | bash"
    exit 1
  fi
}

need_docker() {
  if ! command -v docker &>/dev/null; then
    err "docker is not installed."
    exit 1
  fi
}

# ── infra (postgres / redis / minio) ──────────────────────────────────────

start_infra() {
  need_docker
  info "Ensuring infra is up (postgres, redis, minio)..."
  docker compose -f "$ROOT_DIR/docker-compose.yml" up -d --quiet-pull
  ok "Infra ready."
}

stop_infra() {
  need_docker
  info "Stopping infra..."
  docker compose -f "$ROOT_DIR/docker-compose.yml" down
  ok "Infra stopped."
}

# ── Nexus-Auth background process ─────────────────────────────────────────

auth_pid() {
  [[ -f "$PID_FILE" ]] && cat "$PID_FILE" || echo ""
}

is_auth_running() {
  local pid
  pid="$(auth_pid)"
  [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null
}

start_auth() {
  if is_auth_running; then
    ok "Nexus-Auth already running (pid $(auth_pid))."
    return
  fi

  need_bun

  if [[ ! -d "$AUTH_DIR" ]]; then
    err "apps/Nexus-Auth not found."
    exit 1
  fi

  info "Starting Nexus-Auth (port 4310)..."

  # Install deps if node_modules is missing
  if [[ ! -d "$AUTH_DIR/node_modules" ]]; then
    warn "node_modules missing — running bun install first..."
    (cd "$AUTH_DIR" && bun install --frozen-lockfile --ignore-scripts)
  fi

  (
    cd "$AUTH_DIR"
    nohup bun run src/index.ts >> "$LOG_FILE" 2>&1 &
    echo $! > "$PID_FILE"
  )

  # Give it a moment to fail fast if something is wrong
  sleep 1
  if is_auth_running; then
    ok "Nexus-Auth started (pid $(auth_pid)). Logs: .local/dev/nexus-auth.log"
  else
    err "Nexus-Auth failed to start. Check .local/dev/nexus-auth.log"
    rm -f "$PID_FILE"
    exit 1
  fi
}

stop_auth() {
  local pid
  pid="$(auth_pid)"
  if [[ -z "$pid" ]]; then
    warn "Nexus-Auth is not tracked (no pid file)."
    return
  fi
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid"
    ok "Nexus-Auth stopped (pid $pid)."
  else
    warn "Nexus-Auth pid $pid is stale."
  fi
  rm -f "$PID_FILE"
}

# ── module runner ─────────────────────────────────────────────────────────

run_module() {
  local module="$1"
  local app_dir="$ROOT_DIR/apps/$module"

  if [[ ! -d "$app_dir" ]]; then
    err "Module not found: apps/$module"
    echo ""
    list_modules
    exit 1
  fi

  if [[ ! -f "$app_dir/package.json" ]]; then
    err "$module has no package.json — it may be an empty shell."
    exit 1
  fi

  if ! grep -q '"dev"' "$app_dir/package.json" 2>/dev/null; then
    err "$module has no 'dev' script in package.json."
    exit 1
  fi

  need_bun

  # Install deps if missing
  if [[ ! -d "$app_dir/node_modules" ]]; then
    warn "node_modules missing in $module — running bun install first..."
    (cd "$app_dir" && bun install --frozen-lockfile --ignore-scripts)
  fi

  ok "Starting $module..."
  info "Ctrl+C stops $module. Auth + infra keep running."
  info "Run './dev.sh stop' to also stop auth."
  echo ""

  cd "$app_dir"
  exec bun run dev
}

# ── list ──────────────────────────────────────────────────────────────────

list_modules() {
  echo "Available modules (have a 'dev' script):"
  for d in "$ROOT_DIR"/apps/Nexus-*/; do
    local name
    name="$(basename "$d")"
    if [[ -f "$d/package.json" ]] && grep -q '"dev"' "$d/package.json" 2>/dev/null; then
      local port
      port="$(grep -rh 'PORT\b' "$d/src/" 2>/dev/null | grep -oP '\d{4,5}' | head -1 || true)"
      printf "  %-30s %s\n" "$name" "${port:+:$port}"
    fi
  done
}

# ── status ────────────────────────────────────────────────────────────────

show_status() {
  echo ""
  # infra
  if docker compose -f "$ROOT_DIR/docker-compose.yml" ps --quiet 2>/dev/null | grep -q .; then
    ok "Infra:      running"
  else
    warn "Infra:      stopped"
  fi

  # auth
  if is_auth_running; then
    ok "Nexus-Auth: running (pid $(auth_pid))"
  else
    warn "Nexus-Auth: stopped"
  fi
  echo ""
}

# ── main ──────────────────────────────────────────────────────────────────

CMD="${1:-}"

case "$CMD" in
  infra)
    start_infra
    ;;

  auth)
    start_infra
    start_auth
    show_status
    info "Run './dev.sh stop' to stop auth, './dev.sh down' to stop everything."
    ;;

  stop)
    stop_auth
    ;;

  down)
    stop_auth
    stop_infra
    ;;

  status)
    show_status
    ;;

  list)
    list_modules
    ;;

  logs)
    target="${2:-nexus-auth}"
    log="$STATE_DIR/$target.log"
    if [[ ! -f "$log" ]]; then
      err "No log file: $log"
      exit 1
    fi
    tail -f "$log"
    ;;

  "")
    echo ""
    echo "  Usage: scripts/dev.sh <command>"
    echo ""
    echo "  Commands:"
    echo "    scripts/dev.sh Nexus-Dashboard   start infra + auth + Nexus-Dashboard"
    echo "    scripts/dev.sh auth              start infra + auth only"
    echo "    scripts/dev.sh infra             start docker infra only"
    echo "    scripts/dev.sh stop              stop auth (infra keeps running)"
    echo "    scripts/dev.sh down              stop auth + infra"
    echo "    scripts/dev.sh status            show what's running"
    echo "    scripts/dev.sh list              list available modules"
    echo "    scripts/dev.sh logs [name]       tail logs (default: nexus-auth)"
    echo ""
    exit 1
    ;;

  *)
    # Treat as a module name
    start_infra
    start_auth
    run_module "$CMD"
    ;;
esac
