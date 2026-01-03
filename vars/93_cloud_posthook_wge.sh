#!/bin/bash

USER_VAR_FILE="./etc/profile.d/user_vars.sh"
if [ -f "$USER_VAR_FILE" ]; then
  . "$USER_VAR_FILE"
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
  dnscrypt-proxy -service restart
  popd
}

function run_wge_server() {
  nft -f $WGE_NFT_ALLOW
  wge-server -conf $WGE_SERVER_CONF -key $WGE_SERVER_KEY -cert $WGE_SERVER_CERT -listen 0.0.0.0:59995 -dbus
  nft -f $WGE_NFT_DELETE
}

run_wge_server & sleep 45; modify_dnscrypt_config