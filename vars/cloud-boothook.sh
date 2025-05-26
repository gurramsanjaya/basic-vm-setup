#!/bin/bash

umask 022
DNSCRPT_VER=2.1.8

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

function download_and_setup_dnscrypt() {
  local ARCH=$(get_arch)
  local WRK_DIR="linux-${ARCH}"

  mkdir -p /opt/dnscrypt-proxy/
  pushd /opt/dnscrypt-proxy/

  TAR_FILE="dnscrypt-proxy-linux_${ARCH}-${DNSCRPT_VER}.tar.gz"
  curl -sSLO "https://github.com/DNSCrypt/dnscrypt-proxy/releases/download/${DNSCRPT_VER}/${TAR_FILE}"
  tar -xvf "$TAR_FILE"

  mv "${WRK_DIR}/example-dnscrypt-proxy.toml" "${WRK_DIR}/dnscrypt-proxy.toml"

  # Set the dnscrypt directory as part of env
  cat >> /etc/profile.d/user_vars.sh << EOF
  export DNSCRYPT_HOME=${WRK_DIR}
EOF

  # It determines the symlink source directory to resolve config toml path
  ln -rs "${WRK_DIR}/dnscrypt-proxy" /usr/bin/
  popd
}

# get default network device and set it to /etc/profile.d/ so its available for all users later
export DEFAULT_INTERFACE=$(ip route get 9.9.9.9 | awk '{print $5}')

mkdir -p /etc/profile.d/
cat >> /etc/profile.d/user_vars.sh << EOF
export DEFAULT_INTERFACE=${DEFAULT_INTERFACE}
EOF

# Setup firewall, toggle the nftables service later
# This won't work for redhat variants. For redhat, place this in /etc/sysconfig/nftables.conf or /etc/nftables/main.nft
curl -sSL https://raw.githubusercontent.com/gurramsanjaya/basic-vm-setup/refs/heads/main/vars/nftables.conf | envsubst '$DEFAULT_INTERFACE' > /etc/nftables.conf
nft -f /etc/nftables.conf

# Basic dnscrpt-proxy setup. Change the toml/service later
download_and_setup_dnscrypt