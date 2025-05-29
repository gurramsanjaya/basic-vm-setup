#!/bin/bash

# The main cloud-init process is using initial set of env variables and those are being passed to
# this posthook child process. The env variables set in cloud-boothook.sh need to be manually sourced
USER_VAR_FILE="./etc/profile.d/user_vars.sh"
if [ -f "$USER_VAR_FILE" ]; then
  . "$USER_VAR_FILE"
fi

function disable_systemd_resolved() {
  # TODO: Need to make this atomic using traps
  systemctl disable --now systemd-resolved.service
  cat > /etc/resolv.conf << EOF
# For both ipv4 and ipv6
nameserver 127.0.0.1
nameserver ::1
options edns0 trust-ad
search .
EOF
}

function modify_dnscrypt_config() {

  pushd "$DNSCRYPT_HOME"
  sed -i -E "s/^(listen_addresses = \[)(.*)\$/\\1'\[::1\]:53', \\2/" dnscrypt-proxy.toml
  sed -i -E 's/^# (blocked_names_f.*)$/\1/' dnscrypt-proxy.toml

  # Block DoH queries specified by client, force client to use dnscrypt DNS.
  curl -sSfL https://raw.githubusercontent.com/dibdot/DoH-IP-blocklists/refs/heads/master/doh-domains.txt >> blocked-names.txt

  # Block NSFW domains
  # curl -sSfL https://raw.githubusercontent.com/hagezi/dns-blocklists/refs/heads/main/wildcard/nsfw.txt >> blocked-names.txt

  cat >> blocked-names.txt << EOF
## Add whatever other blocked domains you require here
EOF
  popd
}

systemctl enable --now nftables.service

modify_dnscrypt_config
disable_systemd_resolved
dnscrypt-proxy -service install
dnscrypt-proxy -service start
sleep 5s
dnscrypt-proxy -resolve google.com
