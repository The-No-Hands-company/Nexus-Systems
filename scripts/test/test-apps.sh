#!/bin/bash

# Simple test to verify apps start and health endpoints work
ROOT_DIR="/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems"

APPS_TO_TEST=("Nexus-Graphic" "Nexus-Design" "Nexus-Draw" "Nexus-Photos" "Nexus-Video")
APP_PIDS=()

wait_for_port() {
    port=$1
    name=$2
    for i in $(seq 1 30); do
        if nc -z localhost $port 2>/dev/null; then
            echo "$name is ready on port $port"
            return 0
        fi
        sleep 1
    done
    echo "ERROR: $name failed to start on port $port"
    return 1
}

# Start apps with cloud integration enabled and API key
echo "=== Starting test apps ==="
port=3080
for app in "${APPS_TO_TEST[@]}"; do
    app_dir="$ROOT_DIR/apps/$app"
    env_var="NEXUS_${app//-/_}_ENABLE_CLOUD_INTEGRATION"
    
    export PORT=$port
    export "$env_var=true"
    export NEXUS_CLOUD_URL="http://localhost:8787"
    export NEXUS_CLOUD_API_KEY="change-me"
    
    echo "=== Starting $app on port $port ==="
    cd "$app_dir"
    if command -v bun >/dev/null 2>&1; then
        bun run src/index.ts &
        APP_PIDS+=($!)
        echo "Started $app with PID ${APP_PIDS[-1]} on port $port"
    else
        echo "ERROR: bun not found"
        exit 1
    fi
    port=$((port + 1))
done

# Wait for apps to be ready
echo "=== Waiting for apps to be ready ==="
port=3080
for app in "${APPS_TO_TEST[@]}"; do
    if ! wait_for_port $port "$app"; then
        echo "ERROR: $app failed to start"
        exit 1
    fi
    port=$((port + 1))
done

# Test health endpoints
echo "=== Testing health endpoints ==="
port=3080
for app in "${APPS_TO_TEST[@]}"; do
    echo "Testing $app on port $port"
    curl -s "http://localhost:$port/health" | grep -q '"status":"ok"' && echo "  ✓ Health check passed" || echo "  ✗ Health check failed"
    curl -s "http://localhost:$port/api/v1/status" | grep -q '"service":"nexus-.*"' && echo "  ✓ Status check passed" || echo "  ✗ Status check failed"
    port=$((port + 1))
done

# Stop apps
echo "=== Stopping apps ==="
for pid in "${APP_PIDS[@]}"; do
    if kill -0 $pid 2>/dev/null; then
        kill $pid 2>/dev/null
    fi
done

echo "=== Test completed successfully ==="
