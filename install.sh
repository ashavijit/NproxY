#!/usr/bin/env bash
# Nproxy installer
# Usage:
#   bash install.sh                # install to /usr/local/bin (requires sudo)
#   bash install.sh --local        # install to ./bin/nproxy (no sudo)
#   bash install.sh --systemd      # install + install systemd service
#   bash install.sh --uninstall    # remove installed files

set -euo pipefail

BINARY_NAME="nproxy"
INSTALL_PREFIX="/usr/local"
CONFIG_DIR="/etc/nproxy"
LOG_DIR="/var/log/nproxy"
RUN_DIR="/run/nproxy"
SERVICE_FILE="/etc/systemd/system/nproxy.service"
NPROXY_USER="nproxy"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()    { echo -e "${BLUE}[info]${NC}  $*"; }
success() { echo -e "${GREEN}[ok]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[warn]${NC}  $*"; }
error()   { echo -e "${RED}[error]${NC} $*" >&2; exit 1; }

need_cmd() { command -v "$1" &>/dev/null || error "Required command not found: $1"; }

check_deps() {
    need_cmd gcc
    need_cmd make
    need_cmd pkg-config
    pkg-config --exists openssl || error "OpenSSL development headers not found.
  Install with:
    Ubuntu/Debian: sudo apt install libssl-dev
    Fedora/RHEL:   sudo dnf install openssl-devel
    Arch Linux:    sudo pacman -S openssl"
}

build() {
    info "Building nproxy..."
    make clean >/dev/null 2>&1 || true
    if ! make -j"$(nproc)" >/dev/null 2>&1; then
        make -j"$(nproc)" 2>&1 || true
        error "Build failed. See output above."
    fi
    success "Build complete: ./nproxy ($(du -sh nproxy | cut -f1))"
}

install_local() {
    build
    mkdir -p ./bin
    cp nproxy ./bin/nproxy
    chmod +x ./bin/nproxy
    success "Installed to $(pwd)/bin/nproxy"
    info  "Add to PATH: export PATH=\"\$PATH:$(pwd)/bin\""
}

install_system() {
    build
    info "Installing to ${INSTALL_PREFIX}/bin/${BINARY_NAME} ..."

    # Create system user
    if ! id -u "$NPROXY_USER" &>/dev/null; then
        useradd --system --no-create-home --shell /sbin/nologin "$NPROXY_USER"
        success "Created system user: $NPROXY_USER"
    fi

    # Install binary
    install -Dm755 nproxy "${INSTALL_PREFIX}/bin/${BINARY_NAME}"

    # Install config
    mkdir -p "$CONFIG_DIR"
    if [[ ! -f "${CONFIG_DIR}/nproxy.conf" ]]; then
        install -Dm644 nproxy.conf "${CONFIG_DIR}/nproxy.conf"
        success "Installed config: ${CONFIG_DIR}/nproxy.conf"
    else
        warn "Config already exists, skipping: ${CONFIG_DIR}/nproxy.conf"
    fi

    # Create directories
    mkdir -p "$LOG_DIR" "$RUN_DIR"
    chown "$NPROXY_USER:$NPROXY_USER" "$LOG_DIR" "$RUN_DIR"

    success "Installed: ${INSTALL_PREFIX}/bin/${BINARY_NAME}"
}

install_systemd() {
    install_system

    info "Installing systemd service..."
    cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=Nproxy HTTP Reverse Proxy
Documentation=https://github.com/youruser/nproxy
After=network.target
Wants=network.target

[Service]
Type=forking
User=${NPROXY_USER}
Group=${NPROXY_USER}
ExecStart=${INSTALL_PREFIX}/bin/${BINARY_NAME} -c ${CONFIG_DIR}/nproxy.conf
ExecReload=/bin/kill -HUP \$MAINPID
KillMode=mixed
Restart=on-failure
RestartSec=5s

# Security hardening
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=${LOG_DIR} ${RUN_DIR}
PrivateTmp=yes
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_BIND_SERVICE

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    success "Systemd service installed: nproxy.service"
    echo ""
    echo "  Enable and start:    sudo systemctl enable --now nproxy"
    echo "  Check status:        sudo systemctl status nproxy"
    echo "  Reload config:       sudo systemctl reload nproxy"
    echo "  View logs:           sudo journalctl -u nproxy -f"
}

uninstall() {
    info "Uninstalling nproxy..."

    if systemctl is-active --quiet nproxy 2>/dev/null; then
        systemctl stop nproxy
    fi
    if systemctl is-enabled --quiet nproxy 2>/dev/null; then
        systemctl disable nproxy
    fi
    rm -f "$SERVICE_FILE"
    systemctl daemon-reload 2>/dev/null || true

    rm -f "${INSTALL_PREFIX}/bin/${BINARY_NAME}"
    rm -rf "$CONFIG_DIR"

    if id -u "$NPROXY_USER" &>/dev/null; then
        userdel "$NPROXY_USER" 2>/dev/null || true
    fi

    success "Nproxy uninstalled."
    warn "Log directory preserved: ${LOG_DIR}"
}

main() {
    local mode="system"
    for arg in "$@"; do
        case "$arg" in
            --local)     mode="local" ;;
            --systemd)   mode="systemd" ;;
            --uninstall) mode="uninstall" ;;
            --help|-h)
                echo "Usage: $0 [--local|--systemd|--uninstall]"
                exit 0 ;;
            *) error "Unknown option: $arg" ;;
        esac
    done

    echo ""
    echo "  ███╗   ██╗██████╗ ██████╗  ██████╗ ██╗  ██╗██╗   ██╗"
    echo "  ████╗  ██║██╔══██╗██╔══██╗██╔═══██╗╚██╗██╔╝╚██╗ ██╔╝"
    echo "  ██╔██╗ ██║██████╔╝██████╔╝██║   ██║ ╚███╔╝  ╚████╔╝ "
    echo "  ██║╚██╗██║██╔═══╝ ██╔══██╗██║   ██║ ██╔██╗   ╚██╔╝  "
    echo "  ██║ ╚████║██║     ██║  ██║╚██████╔╝██╔╝ ██╗   ██║   "
    echo "  ╚═╝  ╚═══╝╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝   "
    echo ""

    case "$mode" in
        local)     check_deps; install_local ;;
        systemd)   check_deps; install_systemd ;;
        uninstall) uninstall ;;
        system)    check_deps; install_system ;;
    esac
}

main "$@"
