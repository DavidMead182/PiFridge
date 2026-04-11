#!/usr/bin/env bash
# =============================================================================
# PiFridge Setup & Run Script
# =============================================================================
# Usage:
#   ./pifridge_setup.sh setup   — install & configure everything (run once)
#   ./pifridge_setup.sh run     — start all three processes
#   ./pifridge_setup.sh stop    — stop background processes
# =============================================================================

set -euo pipefail

# --- Configuration -----------------------------------------------------------
USERNAME="${SUDO_USER:-pifridge}"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEB_APP_DIR="$REPO_DIR/src/web_app"
NGINX_CONF_SRC="$REPO_DIR/config/pifridge.conf"
NGINX_SITE="/etc/nginx/sites-available/pifridge"
WWW_DIR="/var/www/pifridge"
SOCKET_DIR="/var/run/pifridge"
SOCKET_FILE="$SOCKET_DIR/pifridge.sock"
PID_FILE="/tmp/pifridge_pids"

# --- Colours -----------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()    { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# =============================================================================
# SETUP
# =============================================================================
cmd_setup() {
    info "Running as user: $USERNAME"
    info "Repo directory:  $REPO_DIR"

    # -- 1. Build -------------------------------------------------------------
    info "Building with CMake..."
    cmake -B "$REPO_DIR/build" "$REPO_DIR"
    cmake --build "$REPO_DIR/build"
    info "Build complete."

    # -- 2. nginx site config -------------------------------------------------
    info "Configuring nginx..."

    [[ -f "$NGINX_CONF_SRC" ]] || error "nginx config not found at $NGINX_CONF_SRC"

    sudo cp "$NGINX_CONF_SRC" "$NGINX_SITE"

    # Replace the root path placeholder with the actual web_app path
    sudo sed -i "s|root .*;|root $WEB_APP_DIR;|g" "$NGINX_SITE"
    info "nginx site root set to: $WEB_APP_DIR"

    # Enable site, disable default
    sudo ln -sf "$NGINX_SITE" /etc/nginx/sites-enabled/pifridge
    sudo rm -f /etc/nginx/sites-enabled/default

    sudo nginx -t || error "nginx config test failed — check $NGINX_SITE"
    sudo systemctl reload nginx
    info "nginx reloaded."

    # -- 3. Web app files -----------------------------------------------------
    info "Copying web app files to $WWW_DIR..."
    sudo mkdir -p "$WWW_DIR"

    # Find index.html — first check expected location, then search the whole repo
    INDEX_HTML="$WEB_APP_DIR/index.html"
    if [[ ! -f "$INDEX_HTML" ]]; then
        warn "index.html not found at $INDEX_HTML, searching repo..."
        INDEX_HTML=$(find "$REPO_DIR" -name "index.html" | head -n 1)
        [[ -n "$INDEX_HTML" ]] || error "Could not find index.html anywhere in $REPO_DIR"
        info "Found index.html at: $INDEX_HTML"
    fi

    sudo cp "$INDEX_HTML" "$WWW_DIR/index.html"
    sudo chown -R www-data:www-data "$WWW_DIR"
    sudo chmod 644 "$WWW_DIR/index.html"
    info "Copied index.html -> $WWW_DIR/index.html"

    if [[ -f "$WWW_DIR/index.html" ]]; then
        info "index.html confirmed at $WWW_DIR/index.html"
    else
        error "Copy seemed to succeed but $WWW_DIR/index.html is missing — check permissions."
    fi

    # -- 4. FastCGI socket directory ------------------------------------------
    info "Setting up FastCGI socket directory at $SOCKET_DIR..."
    sudo mkdir -p "$SOCKET_DIR"
    sudo chown "$USERNAME:www-data" "$SOCKET_DIR"
    sudo chmod 770 "$SOCKET_DIR"
    info "Socket directory ready."

    echo ""
    info "===== Setup complete! ====="
    info "Run './pifridge_setup.sh run' to start PiFridge."
}

# =============================================================================
# RUN
# =============================================================================
cmd_run() {
    info "Starting PiFridge processes..."
    > "$PID_FILE"   # clear old PIDs

    # -- Process 1: main sensor process (needs sudo for I2C/GPIO) ------------
    info "Starting pifridge sensor process (sudo)..."
    sudo "$REPO_DIR/build/src/pifridge" &
    echo "sensor:$!" >> "$PID_FILE"
    info "Sensor process started (PID $!)."

    # Give it a moment to create the socket
    sleep 2

    # -- Process 2: FastCGI API server ----------------------------------------
    info "Starting pifridge_api (FastCGI)..."
    "$REPO_DIR/build/src/web_app/pifridge_api" &
    API_PID=$!
    echo "api:$API_PID" >> "$PID_FILE"
    info "API process started (PID $API_PID)."

    # Wait briefly then fix socket permissions
    sleep 1
    if [[ -S "$SOCKET_FILE" ]]; then
        sudo chown "$USERNAME:www-data" "$SOCKET_FILE"
        sudo chmod 660 "$SOCKET_FILE"
        info "Socket permissions set."
    else
        warn "Socket $SOCKET_FILE not found yet — you may need to fix permissions manually:"
        warn "  sudo chown $USERNAME:www-data $SOCKET_FILE"
        warn "  sudo chmod 660 $SOCKET_FILE"
    fi

    # -- Process 3: nginx -----------------------------------------------------
    info "Ensuring nginx is running..."
    if ! sudo systemctl is-active --quiet nginx; then
        sudo systemctl start nginx
        info "nginx started."
    else
        info "nginx is already running."
    fi

    echo ""
    info "===== PiFridge is running! ====="
    info "Open a browser and go to: http://localhost"
    info ""
    info "PIDs saved to $PID_FILE"
    info "Run './pifridge_setup.sh stop' to shut everything down."
    info ""
    info "Waiting — press Ctrl+C to stop all processes..."

    # Wait and clean up on Ctrl+C
    trap cmd_stop INT TERM
    wait
}

# =============================================================================
# STOP
# =============================================================================
cmd_stop() {
    info "Stopping PiFridge processes..."

    if [[ -f "$PID_FILE" ]]; then
        while IFS=: read -r name pid; do
            if kill -0 "$pid" 2>/dev/null; then
                if [[ "$name" == "sensor" ]]; then
                    sudo kill "$pid" 2>/dev/null && info "Stopped $name (PID $pid)."
                else
                    kill "$pid" 2>/dev/null && info "Stopped $name (PID $pid)."
                fi
            else
                warn "$name (PID $pid) was not running."
            fi
        done < "$PID_FILE"
        rm -f "$PID_FILE"
    else
        warn "No PID file found at $PID_FILE — nothing to stop."
    fi

    info "Done."
}

# =============================================================================
# Entry point
# =============================================================================
case "${1:-help}" in
    setup) cmd_setup ;;
    run)   cmd_run   ;;
    stop)  cmd_stop  ;;
    *)
        echo "Usage: $0 {setup|run|stop}"
        echo ""
        echo "  setup  — build, configure nginx, copy files (run once)"
        echo "  run    — start sensor, API, and nginx"
        echo "  stop   — stop background processes"
        exit 1
        ;;
esac