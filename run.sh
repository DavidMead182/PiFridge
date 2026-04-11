#!/bin/bash
# run.sh
# Builds and runs all PiFridge processes.
# Usage: ./run.sh
# Stop with: Ctrl+C

# ---------------------------------------------------------------------------
# Config — change PI_USER if your Raspberry Pi username is different
# ---------------------------------------------------------------------------
PI_USER="pifridge"

# ---------------------------------------------------------------------------
# Auto-detect repo root from script location
# ---------------------------------------------------------------------------
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> [PiFridge] Repo root:  $REPO_DIR"
echo "==> [PiFridge] Pi user:    $PI_USER"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo ""
echo "==> [PiFridge] Building..."
cmake -B "$REPO_DIR/build" "$REPO_DIR"
cmake --build "$REPO_DIR/build"
echo "==> [PiFridge] Build complete."

# ---------------------------------------------------------------------------
# Copy latest index.html to nginx serve directory
# ---------------------------------------------------------------------------
echo ""
echo "==> [PiFridge] Copying web app files..."
sudo mkdir -p /var/www/pifridge
sudo cp "$REPO_DIR/src/web_app/index.html" /var/www/pifridge/
sudo chown -R www-data:www-data /var/www/pifridge

# ---------------------------------------------------------------------------
# Ensure socket directory exists with correct permissions
# ---------------------------------------------------------------------------
echo ""
echo "==> [PiFridge] Setting up socket directory..."
sudo mkdir -p /var/run/pifridge
sudo chown "$PI_USER":www-data /var/run/pifridge
sudo chmod 770 /var/run/pifridge

# ---------------------------------------------------------------------------
# Ensure database directory exists
# ---------------------------------------------------------------------------
sudo mkdir -p /var/lib/pifridge
sudo chown "$PI_USER":"$PI_USER" /var/lib/pifridge

# ---------------------------------------------------------------------------
# Start nginx
# ---------------------------------------------------------------------------
echo ""
echo "==> [PiFridge] Starting nginx..."
sudo systemctl start nginx

# ---------------------------------------------------------------------------
# Kill any existing instances cleanly before starting fresh
# ---------------------------------------------------------------------------
pkill -x pifridge_api       2>/dev/null || true
pkill -x pifridge_inventory 2>/dev/null || true
sleep 0.5

# ---------------------------------------------------------------------------
# Start FastCGI vitals API in background
# ---------------------------------------------------------------------------
echo "==> [PiFridge] Starting vitals API..."
"$REPO_DIR/build/src/web_app/pifridge_api" &
FCGI_VITALS_PID=$!

sleep 1

if [ -S /var/run/pifridge/pifridge.sock ]; then
    sudo chown "$PI_USER":www-data /var/run/pifridge/pifridge.sock
    sudo chmod 660 /var/run/pifridge/pifridge.sock
    echo "==> [PiFridge] Vitals socket ready."
else
    echo "[ERROR] Vitals socket not created. Check pifridge_api output."
    kill $FCGI_VITALS_PID 2>/dev/null
    exit 1
fi

# ---------------------------------------------------------------------------
# Start FastCGI inventory API in background
# ---------------------------------------------------------------------------
echo "==> [PiFridge] Starting inventory API..."
"$REPO_DIR/build/src/web_app/pifridge_inventory" &
FCGI_INVENTORY_PID=$!

sleep 1

if [ -S /var/run/pifridge/pifridge_inventory.sock ]; then
    sudo chown "$PI_USER":www-data /var/run/pifridge/pifridge_inventory.sock
    sudo chmod 660 /var/run/pifridge/pifridge_inventory.sock
    echo "==> [PiFridge] Inventory socket ready."
else
    echo "[ERROR] Inventory socket not created. Check pifridge_inventory output."
    kill $FCGI_VITALS_PID $FCGI_INVENTORY_PID 2>/dev/null
    exit 1
fi

# ---------------------------------------------------------------------------
# Cleanup trap — kills background processes on Ctrl+C
# ---------------------------------------------------------------------------
cleanup() {
    echo ""
    echo "==> [PiFridge] Shutting down..."
    kill $FCGI_VITALS_PID    2>/dev/null
    kill $FCGI_INVENTORY_PID 2>/dev/null
    echo "==> [PiFridge] Stopped."
    exit 0
}
trap cleanup SIGINT SIGTERM

# ---------------------------------------------------------------------------
# Start main pifridge process in foreground (needs sudo for I2C/GPIO)
# ---------------------------------------------------------------------------
echo ""
echo "==> [PiFridge] Starting main sensor process..."
echo "==> [PiFridge] Web app available at http://localhost"
echo "    Press Ctrl+C to stop all processes."
echo ""

sudo "$REPO_DIR/build/src/pifridge"