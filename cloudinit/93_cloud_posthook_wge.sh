#!/bin/bash

# The main cloud-init process is using initial set of env variables and those are being passed to
# this posthook child process. The env variables set in cloud-boothook.sh need to be manually sourced
USER_VARS_FILE="/etc/profile.d/user_vars.sh"
if [ -f "$USER_VARS_FILE" ]; then
  . "$USER_VARS_FILE"
fi

WGE_ETC_DIR="/etc/wge"
WGE_NFT_ALLOW="${WGE_ETC_DIR}/wge_nft_allow.conf"
WGE_NFT_DELETE="${WGE_ETC_DIR}/wge_nft_delete.conf"
WGE_SERVER_CONF="${WGE_ETC_DIR}/server.toml"
WGE_SERVER_KEY="${WGE_ETC_DIR}/server.key"
WGE_SERVER_CERT="${WGE_ETC_DIR}/server.pem"

function modify_dnscrypt_config() {
  ## Add the wireguard server as dns listen addresses
  pushd "$DNSCRYPT_HOME"
  sed -i -E "s/^(listen_addresses = \[)(.*)\$/\\1'192.168.10.1:53', \\2/" dnscrypt-proxy.toml
  sed -i -E "s/^(listen_addresses = \[)(.*)\$/\\1'\[fd00:10::1\]:53', \\2/" dnscrypt-proxy.toml
  systemctl restart dnscrypt-proxy
  popd
}

function run_wge_server() {
  nft -f $WGE_NFT_ALLOW
  wge-server -conf $WGE_SERVER_CONF -key $WGE_SERVER_KEY -cert $WGE_SERVER_CERT -listen 0.0.0.0:59995 -dbus
  nft -f $WGE_NFT_DELETE
}

run_wge_server & sleep 45; modify_dnscrypt_config