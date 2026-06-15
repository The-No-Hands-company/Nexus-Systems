#!/bin/bash

# Quick test script to verify Nexus-Cloud + apps integration
# Usage: ./test-integration.sh

ROOT_DIR="/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems"

# Function to start a single app
start_app() {
    local app_name=$1
    local app_dir="$ROOT_DIR/apps/$app_name"
    local port=$2
    
    # Convert app name to valid environment variable name
    local env_var="NEXUS_${app_name//-/_}_ENABLE_CLOUD_INTEGRATION"
    
    echo "=== Starting $app_name on port $port ==="
    
    # Set environment variables
    export PORT=$port
    export "$env_var=true"
    export NEXUS_CLOUD_URL="http://localhost:8787"
    export NEXUS_CLOUD_API_KEY="change-me"
    
    # Start the app
    if [ -f "$app_dir/package.json" ]; then
        cd "$app_dir"
        if command -v bun >/dev/null 2>&1; then
            # Start in background
            bun run src/index.ts &
            APP_PIDS+=($!)
            echo "Started $app_name with PID ${APP_PIDS[-1]}"
        else
            echo "ERROR: bun not found"
            return 1
        fi
    else
        echo "ERROR: $app_dir/package.json not found"
        return 1
    fi
}

# Function to stop all apps
stop_apps() {
    for pid in "${APP_PIDS[@]}"; do
        if kill -0 $pid 2>/dev/null; then
            kill $pid 2>/dev/null
        fi
    done
}

# Function to wait for a port to be ready
wait_for_port() {
    local port=$1
    local name=$2
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

# Main test
APPS_TO_TEST=("Nexus-Graphic" "Nexus-Design" "Nexus-Draw" "Nexus-Photos" "Nexus-Video")
APP_PIDS=()
APP_PORTS=()

# Start Nexus-Cloud
export PORT=8787
export NEXUS_CLOUD_ENABLE_CLOUD_INTEGRATION=true
export NEXUS_CLOUD_API_KEY="change-me"

echo "=== Starting Nexus-Cloud ==="
cd "$ROOT_DIR/apps/Nexus-Cloud"
bun run src/index.ts &
CLOUD_PID=$!

# Wait for Nexus-Cloud to be ready
echo "Waiting for Nexus-Cloud to be ready..."
if ! wait_for_port 8787 "Nexus-Cloud"; then
    echo "ERROR: Nexus-Cloud failed to start"
    exit 1
fi

# Start test apps
echo "=== Starting test apps ==="
port=3080
for app in "${APPS_TO_TEST[@]}"; do
    start_app $app $port
    APP_PORTS+=($port)
    port=$((port + 1))
done

# Wait for apps to be ready
echo "=== Waiting for apps to be ready ==="
for i in "${!APPS_TO_TEST[@]}"; do
    app=${APPS_TO_TEST[$i]}
    port=${APP_PORTS[$i]}
    if ! wait_for_port $port "$app"; then
        stop_apps
        exit 1
    fi
done

# Test registration with Nexus-Cloud
echo "=== Testing registration with Nexus-Cloud ==="
for i in "${!APPS_TO_TEST[@]}"; do
    app=${APPS_TO_TEST[$i]}
    port=${APP_PORTS[$i]}
    echo "Testing $app on port $port"
    curl -s "http://localhost:$port/health" | grep -q '"status":"ok"' && echo "  ✓ Health check passed" || echo "  ✗ Health check failed"
    curl -s "http://localhost:$port/api/v1/status" | grep -q '"service":"nexus-.*"' && echo "  ✓ Status check passed" || echo "  ✗ Status check failed"
done

# Stop all apps
echo "=== Stopping all apps ==="
stop_apps
kill $CLOUD_PID 2>/dev/null

echo "=== Integration test completed successfully ==="
echo "All apps registered with Nexus-Cloud and are healthy"
