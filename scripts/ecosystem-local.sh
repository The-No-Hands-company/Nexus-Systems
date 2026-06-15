#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATE_DIR="$ROOT_DIR/.local/ecosystem"
PID_DIR="$STATE_DIR/pids"
LOG_DIR="$STATE_DIR/logs"
mkdir -p "$PID_DIR" "$LOG_DIR"

if [[ -t 1 ]]; then
  COLOR_GREEN="\033[32m"
  COLOR_YELLOW="\033[33m"
  COLOR_RED="\033[31m"
  COLOR_RESET="\033[0m"
else
  COLOR_GREEN=""
  COLOR_YELLOW=""
  COLOR_RED=""
  COLOR_RESET=""
fi

# Comma-separated list of service names to skip, for example:
# NEXUS_ECOSYSTEM_SKIP="Nexus,Nexus-Computer-frontend"
SKIP_LIST="${NEXUS_ECOSYSTEM_SKIP:-}"

is_skipped() {
  local name="$1"
  if [[ -z "$SKIP_LIST" ]]; then
    return 1
  fi
  IFS=',' read -r -a items <<< "$SKIP_LIST"
  for item in "${items[@]}"; do
    if [[ "${item}" == "$name" ]]; then
      return 0
    fi
  done
  return 1
}

# service|type|workdir|command
SERVICES=(
  "Nexus-Cloud|process|Nexus-Cloud|bun run src/index.ts"
  "Nexus-Auth|process|Nexus-Auth|bun run src/index.ts"
  "Nexus-Guardian|process|Nexus-Guardian|bun run src/index.ts"
  "Nexus-Tunnel|process|Nexus-Tunnel|bun run src/index.ts"
  "Nexus-Edge|process|Nexus-Edge|bun run src/index.ts"
  "Nexus-AI|compose|Nexus-AI|docker compose up -d"
  "Nexus-Hosting|compose|Nexus-Hosting|docker compose up -d"
  "Nexus|process|Nexus|./dev.sh"
  "Nexus-Deploy|process|Nexus-Deploy|npm run dev"
  "Nexus-Network|process|Nexus-Network|npm run dev"
  "Nexus-Vault|process|Nexus-Vault|npm run dev"
  "Nexus-Porter|process|Nexus-Porter|npm run dev"
  "Nexusclaw|process|Nexusclaw|npm run dev"
  "Nit|process|Nit|bun run dev"
  "Nexus-Computer-backend|process|Nexus-Computer/backend|uvicorn main:app --reload"
  "Nexus-Computer-frontend|process|Nexus-Computer/frontend|npm run dev"
)

# name|url
HEALTH_ENDPOINTS=(
  "Nexus-Cloud Health|http://localhost:8787/health"
  "Nexus-Cloud Discovery|http://localhost:8787/.well-known/nexus-cloud"
  "Nexus-Cloud Tools|http://localhost:8787/api/v1/tools"
  "Nexus-Auth Health|http://localhost:4310/health"
  "Nexus-Auth Readiness|http://localhost:4310/api/v1/auth/readiness"
  "Nexus-Guardian Health|http://localhost:4320/health"
  "Nexus-Guardian Readiness|http://localhost:4320/api/v1/guardian/readiness"
  "Nexus-Tunnel Health|http://localhost:4330/health"
  "Nexus-Tunnel Readiness|http://localhost:4330/api/v1/tunnel/readiness"
  "Nexus-Edge Health|http://localhost:4340/health"
  "Nexus-Edge Readiness|http://localhost:4340/api/v1/edge/readiness"
  "Nexus-AI Models|http://localhost:8000/v1/models"
  "Nexus-Hosting API|http://localhost:8080/health"
  "Nexus-Deploy API|http://localhost:3000/health"
  "Nexus-Network UI|http://localhost:3002/"
  "Nexus-Vault API|http://localhost:3001/health"
  "Nexus-Computer Backend|http://localhost:8001/health"
  "Nexus-Computer Readiness|http://localhost:8001/api/v1/computer/readiness"
  "Nexus-Computer Frontend|http://localhost:5173/"
)

pid_file() {
  local name="$1"
  echo "$PID_DIR/${name}.pid"
}

log_file() {
  local name="$1"
  echo "$LOG_DIR/${name}.log"
}

resolve_workdir() {
  local workdir="$1"
  local direct="$ROOT_DIR/$workdir"
  if [[ -d "$direct" ]]; then
    echo "$direct"
    return 0
  fi

  local nested="$ROOT_DIR/apps/$workdir"
  if [[ -d "$nested" ]]; then
    echo "$nested"
    return 0
  fi

  return 1
}

is_running() {
  local name="$1"
  local pf
  pf="$(pid_file "$name")"
  if [[ ! -f "$pf" ]]; then
    return 1
  fi

  local pid
  pid="$(cat "$pf")"
  if [[ -z "$pid" ]]; then
    return 1
  fi

  if kill -0 "$pid" 2>/dev/null; then
    return 0
  fi

  rm -f "$pf"
  return 1
}

start_process_service() {
  local name="$1"
  local workdir="$2"
  local command="$3"

  if is_running "$name"; then
    echo "[skip] $name already running (pid $(cat "$(pid_file "$name")"))"
    return 0
  fi

  local full_dir
  if ! full_dir="$(resolve_workdir "$workdir")"; then
    echo "[warn] $name missing directory: $ROOT_DIR/$workdir or $ROOT_DIR/apps/$workdir"
    return 0
  fi

  local lf
  lf="$(log_file "$name")"

  (
    cd "$full_dir"
    nohup bash -lc "$command" > "$lf" 2>&1 &
    echo $! > "$(pid_file "$name")"
  )

  echo "[ok] started $name"
}

start_compose_service() {
  local name="$1"
  local workdir="$2"
  local command="$3"

  local full_dir
  if ! full_dir="$(resolve_workdir "$workdir")"; then
    echo "[warn] $name missing directory: $ROOT_DIR/$workdir or $ROOT_DIR/apps/$workdir"
    return 0
  fi

  local lf
  lf="$(log_file "$name")"

  (
    cd "$full_dir"
    bash -lc "$command" > "$lf" 2>&1
  ) || {
    echo "[warn] compose start failed for $name (see $lf)"
    return 0
  }

  echo "[ok] started $name via docker compose"
}

start_all() {
  echo "Starting local Nexus ecosystem (Cloud-first order)..."
  for row in "${SERVICES[@]}"; do
    IFS='|' read -r name type workdir command <<< "$row"

    if is_skipped "$name"; then
      echo "[skip] $name in NEXUS_ECOSYSTEM_SKIP"
      continue
    fi

    if [[ "$type" == "process" ]]; then
      start_process_service "$name" "$workdir" "$command"
    else
      start_compose_service "$name" "$workdir" "$command"
    fi
  done

  echo
  status_all
  echo
  echo "Tip: tail logs with: tail -f .local/ecosystem/logs/<service>.log"
}

stop_process_service() {
  local name="$1"
  local pf
  pf="$(pid_file "$name")"

  if [[ ! -f "$pf" ]]; then
    echo "[skip] $name not running"
    return 0
  fi

  local pid
  pid="$(cat "$pf")"
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    echo "[ok] stopped $name (pid $pid)"
  else
    echo "[skip] $name stale pid file"
  fi

  rm -f "$pf"
}

stop_compose_service() {
  local name="$1"
  local workdir="$2"
  local full_dir
  if ! full_dir="$(resolve_workdir "$workdir")"; then
    echo "[skip] $name missing directory"
    return 0
  fi

  (
    cd "$full_dir"
    docker compose down > /dev/null 2>&1 || true
  )
  echo "[ok] stopped $name docker compose"
}

stop_all() {
  echo "Stopping local Nexus ecosystem..."
  for row in "${SERVICES[@]}"; do
    IFS='|' read -r name type workdir command <<< "$row"

    if [[ "$type" == "process" ]]; then
      stop_process_service "$name"
    else
      stop_compose_service "$name" "$workdir"
    fi
  done
}

status_all() {
  echo "Service status"
  for row in "${SERVICES[@]}"; do
    IFS='|' read -r name type workdir command <<< "$row"
    if [[ "$type" == "process" ]]; then
      if is_running "$name"; then
        echo "[up]   $name (pid $(cat "$(pid_file "$name")"))"
      else
        echo "[down] $name"
      fi
    else
      echo "[info] $name managed by docker compose ($workdir)"
    fi
  done
}

classify_probe() {
  local code="$1"
  if [[ "$code" =~ ^2[0-9][0-9]$ || "$code" =~ ^3[0-9][0-9]$ ]]; then
    echo "GREEN|$COLOR_GREEN"
  elif [[ "$code" != "000" ]]; then
    echo "YELLOW|$COLOR_YELLOW"
  else
    echo "RED|$COLOR_RED"
  fi
}

health_all() {
  local green=0
  local yellow=0
  local red=0
  local probe
  local code
  local duration
  local state
  local color
  local classified

  echo "Ecosystem endpoint readiness"
  printf "%-26s %-8s %-6s %-9s %s\n" "SERVICE" "STATE" "HTTP" "LATENCY" "URL"
  printf "%-26s %-8s %-6s %-9s %s\n" "--------------------------" "--------" "------" "---------" "---"

  for row in "${HEALTH_ENDPOINTS[@]}"; do
    IFS='|' read -r name url <<< "$row"

    probe="$(curl -sS -o /dev/null --connect-timeout 1 --max-time 3 -w '%{http_code} %{time_total}' "$url" 2>/dev/null || echo '000 0.000')"
    code="${probe%% *}"
    duration="${probe##* }"
    classified="$(classify_probe "$code")"
    state="${classified%%|*}"
    color="${classified##*|}"

    printf "%-26s %b%-8s%b %-6s %-9s %s\n" "$name" "$color" "$state" "$COLOR_RESET" "$code" "$duration" "$url"

    if [[ "$state" == "GREEN" ]]; then
      green=$((green + 1))
    elif [[ "$state" == "YELLOW" ]]; then
      yellow=$((yellow + 1))
    else
      red=$((red + 1))
    fi
  done

  echo
  printf "Summary: %bGREEN=%d%b %bYELLOW=%d%b %bRED=%d%b\n" \
    "$COLOR_GREEN" "$green" "$COLOR_RESET" \
    "$COLOR_YELLOW" "$yellow" "$COLOR_RESET" \
    "$COLOR_RED" "$red" "$COLOR_RESET"
}

usage() {
  cat <<EOF
Usage: $(basename "$0") <start|stop|restart|status>

Commands:
  start    Start Nexus-Cloud first, then the rest of the local ecosystem
  stop     Stop all services started by this script (and compose stacks)
  restart  Stop then start
  status   Show local service status
  health   Ping known local endpoints and print readiness table

Optional:
  NEXUS_ECOSYSTEM_SKIP="Nexus,Nexus-Computer-frontend" $(basename "$0") start
EOF
}

case "${1:-}" in
  start)
    start_all
    ;;
  stop)
    stop_all
    ;;
  restart)
    stop_all
    start_all
    ;;
  status)
    status_all
    ;;
  health)
    health_all
    ;;
  *)
    usage
    exit 1
    ;;
esac
