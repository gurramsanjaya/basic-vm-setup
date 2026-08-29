#!/bin/bash

umask 022
DNSCRPT_VER=2.1.8
WGE_VER=1.0.12
# this is the array that will hold the any custom vars
USER_VARS=()
USER_VARS_SCRIPT="/etc/profile.d/user_vars.sh"

# this custom.template file won't be picked up by the cron because it has the '.' in it
# (refer: https://www.man7.org/linux/man-pages/man8/cron.8.html#CLUSTERING_SUPPORT)
USER_CRON_TEMPLATE="/etc/cron.d/custom.template"

mkdir -p /etc/profile.d/

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

  local TAR_FILE="dnscrypt-proxy-linux_${ARCH}-${DNSCRPT_VER}.tar.gz"
  curl -sSLO "https://github.com/DNSCrypt/dnscrypt-proxy/releases/download/${DNSCRPT_VER}/${TAR_FILE}"
  tar -xvf "$TAR_FILE"

  mv "${WRK_DIR}/example-dnscrypt-proxy.toml" "${WRK_DIR}/dnscrypt-proxy.toml"

  # Set the dnscrypt directory as part of env
  readarray -t -O "${#USER_VARS[@]}" USER_VARS << EOF
DNSCRYPT_HOME="/opt/dnscrypt-proxy/${WRK_DIR}"
EOF

  # It determines the symlink source directory to resolve config toml path
  ln -rs "${WRK_DIR}/dnscrypt-proxy" /usr/bin/
  popd
}

function download_and_setup_wg_exchange() {
  case $(uname -m) in
    x86_64|amd64)
      ARCH=amd64
      ;;
    arm64)
      ARCH=arm64
      ;;
    *)
      echo "invalid arch for wg-exchange"
      exit 1
      ;;
  esac

  local WGE_HOME="/opt/wg-exchange"

  mkdir -p $WGE_HOME
  pushd $WGE_HOME

  local TAR_FILE="wg-exchange-linux-${ARCH}-${WGE_VER}.tar.zst"
  curl -sSLO "https://github.com/gurramsanjaya/wg-exchange/releases/download/v${WGE_VER}/${TAR_FILE}"
  tar -xvf "$TAR_FILE"

  readarray -t -O "${#USER_VARS[@]}" USER_VARS << EOF
WGE_HOME="${WGE_HOME}"
EOF

  ln -rs "wge-server" /usr/bin/
  popd
}

# Basic dnscrpt-proxy setup. Change the toml/service later
download_and_setup_dnscrypt

download_and_setup_wg_exchange

# setup basic template cron.d file with variables (you can copy and create a proper crontab file from it)
mkdir -p /etc/cron.d/
cat > "$USER_CRON_TEMPLATE" << EOF
SHELL=bash
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
MAILTO=""
EOF

# setup custom variables to both crontab and the profiled script
for var in "${USER_VARS[@]}"; do
  echo "export $var" >> "$USER_VARS_SCRIPT"
  echo "$var" >> "$USER_CRON_TEMPLATE"
done
