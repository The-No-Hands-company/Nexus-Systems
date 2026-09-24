#!/usr/bin/env bash

# Conservative ownership checks for services managed by deploy.sh. A PID file
# is only evidence after the listening socket, process lifetime, working
# directory and command line all agree on the same process.

SS_BIN="${SS_BIN:-ss}"
READLINK_BIN="${READLINK_BIN:-readlink}"
PROC_ROOT="${PROC_ROOT:-/proc}"

process_error() {
    printf '%s\n' "$*" >&2
}

valid_port() {
    case "$1" in
        ''|*[!0-9]*) return 1 ;;
        *) return 0 ;;
    esac
}

valid_service_name() {
    case "$1" in
        ''|*[!a-zA-Z0-9_-]*) return 1 ;;
        *) return 0 ;;
    esac
}

listener_pid() {
    local port=$1
    local listeners pid_lines
    local -a pids

    if ! valid_port "$port"; then
        process_error "invalid port: $port"
        return 1
    fi

    if ! listeners="$("$SS_BIN" -H -ltnp "sport = :$port" 2>/dev/null)"; then
        process_error "could not inspect listeners on :$port"
        return 1
    fi

    pid_lines="$(printf '%s\n' "$listeners" | grep -oE 'pid=[0-9]+' | cut -d= -f2 | sort -nu || true)"
    if [ -z "$pid_lines" ]; then
        process_error "no attributable listener PID on :$port"
        return 1
    fi

    mapfile -t pids <<< "$pid_lines"
    if [ "${#pids[@]}" -ne 1 ]; then
        process_error "ambiguous listener PIDs on :$port"
        return 1
    fi

    printf '%s\n' "${pids[0]}"
}

port_has_listener() {
    local port=$1
    local listeners

    if ! valid_port "$port"; then
        process_error "invalid port: $port"
        return 2
    fi
    if ! listeners="$("$SS_BIN" -H -ltnp "sport = :$port" 2>/dev/null)"; then
        process_error "could not inspect listeners on :$port"
        return 2
    fi
    [ -n "$listeners" ]
}

listener_state() {
    local port=$1
    local result

    if port_has_listener "$port"; then
        printf '%s\n' occupied
        return 0
    else
        result=$?
    fi
    case "$result" in
        1) printf '%s\n' absent ;;
        *) printf '%s\n' unverifiable ;;
    esac
}

pid_matches_service() {
    local pid=$1
    local port=$2
    local expected_dir=$3
    local exec_pattern=$4
    local listening_pid

    case "$pid" in
        ''|*[!0-9]*) process_error "invalid PID: $pid"; return 1 ;;
    esac
    if [ -z "$expected_dir" ] || [ -z "$exec_pattern" ]; then
        process_error "service ownership check requires a directory and executable pattern"
        return 1
    fi

    if ! listening_pid="$(listener_pid "$port")"; then
        return 1
    fi
    if [ "$listening_pid" != "$pid" ]; then
        process_error "PID $pid does not own :$port"
        return 1
    fi
    pid_process_identity_matches "$pid" "$expected_dir" "$exec_pattern"
}

pid_process_identity_matches() {
    local pid=$1
    local expected_dir=$2
    local exec_pattern=$3
    local cwd cmdline

    case "$pid" in
        ''|*[!0-9]*) process_error "invalid PID: $pid"; return 1 ;;
    esac
    if [ -z "$expected_dir" ] || [ -z "$exec_pattern" ]; then
        process_error "service ownership check requires a directory and executable pattern"
        return 1
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        process_error "PID $pid is not live"
        return 1
    fi
    if ! cwd="$("$READLINK_BIN" -f "$PROC_ROOT/$pid/cwd" 2>/dev/null)"; then
        process_error "could not resolve working directory for PID $pid"
        return 1
    fi
    if [ "$cwd" != "$expected_dir" ]; then
        process_error "PID $pid has an unexpected working directory"
        return 1
    fi
    if ! cmdline="$(tr '\0' ' ' < "$PROC_ROOT/$pid/cmdline" 2>/dev/null)"; then
        process_error "could not inspect command line for PID $pid"
        return 1
    fi
    case "$cmdline" in
        *"$exec_pattern"*) ;;
        *) process_error "PID $pid has an unexpected command line"; return 1 ;;
    esac
}

write_pid_file() {
    local name=$1
    local pid=$2
    local temp

    if ! valid_service_name "$name"; then
        process_error "invalid service name: $name"
        return 1
    fi
    case "$pid" in
        ''|*[!0-9]*) process_error "invalid PID: $pid"; return 1 ;;
    esac
    if [ -z "${PID_DIR:-}" ]; then
        process_error "PID_DIR is not set"
        return 1
    fi

    (
        umask 077
        mkdir -p "$PID_DIR" || exit 1
        temp="$(mktemp "$PID_DIR/.${name}.pid.XXXXXX")" || exit 1
        trap 'rm -f "$temp"' EXIT
        printf '%s\n' "$pid" > "$temp" || exit 1
        mv -f "$temp" "$PID_DIR/$name.pid" || exit 1
        trap - EXIT
    ) || {
        process_error "could not update PID file for $name"
        return 1
    }
}

reconcile_pid() {
    local name=$1
    local port=$2
    local expected_dir=$3
    local exec_pattern=$4
    local pid

    if ! pid="$(listener_pid "$port")"; then
        return 1
    fi
    if ! pid_matches_service "$pid" "$port" "$expected_dir" "$exec_pattern"; then
        return 1
    fi
    if ! write_pid_file "$name" "$pid"; then
        return 1
    fi

    printf '%s\n' "$pid"
}

validated_pid() {
    local name=$1
    local port=$2
    local expected_dir=$3
    local exec_pattern=$4
    local pid extra
    local path="${PID_DIR:-}/$name.pid"

    if ! valid_service_name "$name"; then
        process_error "invalid service name: $name"
        return 1
    fi
    if [ ! -f "$path" ]; then
        process_error "no PID file for $name"
        return 1
    fi
    if ! IFS= read -r pid < "$path"; then
        process_error "empty PID file for $name"
        return 1
    fi
    if IFS= read -r extra < <(sed -n '2p' "$path"); then
        if [ -n "$extra" ]; then
            process_error "malformed PID file for $name"
            return 1
        fi
    fi
    case "$pid" in
        ''|*[!0-9]*) process_error "invalid PID file for $name"; return 1 ;;
    esac
    if ! pid_matches_service "$pid" "$port" "$expected_dir" "$exec_pattern"; then
        return 1
    fi

    printf '%s\n' "$pid"
}
