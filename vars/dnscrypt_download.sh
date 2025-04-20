#!/bin/bash
umask 754

# Basic dnscrpt-proxy setup. Change the toml/service later
VER=2.1.8

function get_arch() {
    case "$(uname -m)" in 
        x86_64|amd64)
            echo "x86_64"
            ;;
        i?86)
            echo "i386"
            ;;
        arm64)
            echo "arm64"
            ;;
        arm)
            echo "arm"
            ;;
        *)
            echo "Can't determine Architecture"
            exit 1
        esac
}

function download_and_setup() {
    ARCH=$(get_arch)
    TAR_FILE="dnscrypt-proxy-linux_${ARCH}-${VER}.tar.gz"
    curl -sSLO "https://github.com/DNSCrypt/dnscrypt-proxy/releases/download/${VER}/${TAR_FILE}"
    tar -xvf "$TAR_FILE"
}

function disable_systemd_resolved() {
    # Disable systemd-resolved only after downloading the tar file
    systemctl disable --now systemd-resolved.service
    cat > /etc/resolv.conf << EOF
nameserver 127.0.0.1
options edns0 trust-ad
search .
EOF
}

mkdir -p /opt/dnscrypt/
cd /opt/dnscrypt/

download_and_setup
disable_systemd_resolved

cd "linux-${ARCH}"
mv example-dnscrypt-proxy.toml dnscrypt-proxy.toml
./dnscript-proxy -service start
./dnscrypt-proxy -resolve google.com

