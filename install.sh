#!/usr/bin/env bash
set -euo pipefail

# ─── Nproxy Installer ────────────────────────────────────────────────────────
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/ashavijit/NproxY/main/install.sh | bash
#   bash install.sh --local          # install to ./bin/nproxy
#   sudo bash install.sh --systemd   # install + create systemd service

REPO="ashavijit/NproxY"
VERSION="latest"
PREFIX="/usr/local"
LOCAL=false
SYSTEMD=false

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${CYAN}▸${NC} $*"; }
ok()    { echo -e "${GREEN}✓${NC} $*"; }
err()   { echo -e "${RED}✗${NC} $*" >&2; exit 1; }

for arg in "$@"; do
  case "$arg" in
    --local)   LOCAL=true ;;
    --systemd) SYSTEMD=true ;;
    --help|-h)
      echo "Usage: install.sh [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --local     Install to ./bin/nproxy (no root required)"
      echo "  --systemd   Install system-wide + create systemd service"
      echo "  --help      Show this help"
      exit 0
      ;;
  esac
done

# ─── Detect platform ─────────────────────────────────────────────────────────
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$ARCH" in
  x86_64)  ARCH="amd64" ;;
  aarch64) ARCH="arm64" ;;
  *)       err "Unsupported architecture: $ARCH" ;;
esac

if [ "$OS" != "linux" ]; then
  err "Nproxy only supports Linux (got: $OS)"
fi

# ─── Check dependencies ──────────────────────────────────────────────────────
check_deps() {
  local missing=()
  command -v gcc   >/dev/null 2>&1 || missing+=("gcc (build-essential)")
  command -v make  >/dev/null 2>&1 || missing+=("make")
  [ -f /usr/include/openssl/ssl.h ] || [ -f /usr/include/x86_64-linux-gnu/openssl/ssl.h ] || missing+=("libssl-dev")
  dpkg -s zlib1g-dev >/dev/null 2>&1 || [ -f /usr/include/zlib.h ] || missing+=("zlib1g-dev")

  if [ ${#missing[@]} -gt 0 ]; then
    echo ""
    info "Missing dependencies: ${missing[*]}"
    echo ""
    echo "  Ubuntu/Debian:  sudo apt install build-essential libssl-dev zlib1g-dev"
    echo "  Fedora/RHEL:    sudo dnf install gcc make openssl-devel zlib-devel"
    echo "  Arch:           sudo pacman -S base-devel openssl zlib"
    echo ""
    err "Install dependencies first, then re-run this script."
  fi
}

# ─── Build from source ───────────────────────────────────────────────────────
build() {
  info "Building nproxy from source..."

  if [ ! -f Makefile ]; then
    info "Cloning repository..."
    git clone --depth 1 "https://github.com/${REPO}.git" /tmp/nproxy-build
    cd /tmp/nproxy-build
  fi

  make clean >/dev/null 2>&1 || true
  make -j"$(nproc)" 2>&1

  if [ ! -f nproxy ]; then
    err "Build failed — nproxy binary not found."
  fi

  ok "Build complete ($(du -h nproxy | cut -f1) binary)"
}

# ─── Install ──────────────────────────────────────────────────────────────────
install_local() {
  mkdir -p bin
  cp nproxy bin/nproxy
  chmod +x bin/nproxy
  ok "Installed to ./bin/nproxy"
  echo ""
  echo "  Run:   ./bin/nproxy -c vhosts.conf -w"
  echo "  Test:  ./bin/nproxy -t"
}

install_system() {
  local bindir="${PREFIX}/bin"

  if [ "$(id -u)" -ne 0 ]; then
    err "System install requires root. Run: sudo bash install.sh"
  fi

  mkdir -p "$bindir"
  cp nproxy "$bindir/nproxy"
  chmod +x "$bindir/nproxy"

  mkdir -p /etc/nproxy /var/log/nproxy
  [ -f /etc/nproxy/nproxy.conf ] || cp vhosts.conf /etc/nproxy/nproxy.conf 2>/dev/null || true

  ok "Installed to ${bindir}/nproxy"
  ok "Config at /etc/nproxy/nproxy.conf"
}

install_systemd() {
  install_system

  cat > /etc/systemd/system/nproxy.service <<'EOF'
[Unit]
Description=Nproxy reverse proxy
After=network-online.target
Wants=network-online.target

[Service]
Type=forking
PIDFile=/run/nproxy.pid
ExecStart=/usr/local/bin/nproxy -c /etc/nproxy/nproxy.conf -d
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/bin/kill -TERM $MAINPID
Restart=on-failure
RestartSec=5
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
EOF

  systemctl daemon-reload
  ok "Systemd service created"
  echo ""
  echo "  Enable:  sudo systemctl enable --now nproxy"
  echo "  Status:  sudo systemctl status nproxy"
  echo "  Reload:  sudo systemctl reload nproxy"
}

# ─── Main ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}  ┌─────────────────────────────┐${NC}"
echo -e "${BOLD}  │     Nproxy Installer         │${NC}"
echo -e "${BOLD}  └─────────────────────────────┘${NC}"
echo ""

check_deps
build

if $SYSTEMD; then
  install_systemd
elif $LOCAL; then
  install_local
else
  install_system
fi

echo ""
ok "Done! Run ${BOLD}nproxy --help${NC} to get started."
echo ""
