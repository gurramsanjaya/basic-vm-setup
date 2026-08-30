#!/bin/bash

# The main cloud-init process is using initial set of env variables and those are being passed to
# this posthook child process. The env variables set in cloud-boothook.sh need to be manually sourced
USER_VARS_FILE="/etc/profile.d/user_vars.sh"
if [ -f "$USER_VARS_FILE" ]; then
  . "$USER_VARS_FILE"
fi
# the cron template file setup in the boothook
USER_CRON_TEMPLATE="/etc/cron.d/custom.template"
RB_CRON_FILE="/etc/cron.d/dnscrypt_blocklist"
RB_SCRIPT_FILE="/etc/blocklist/refresh_blocklist.sh"

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
  sed -i -E 's/^(ipv6_servers\s*=\s*)false/\1true/' dnscrypt-proxy.toml
  sed -i -E 's/^# (blocked_names_f.*)$/\1/' dnscrypt-proxy.toml

  touch blocked-names.txt

  # Using aggregated list
  # curl -sSfL https://raw.githubusercontent.com/gurramsanjaya/basic-vm-setup/main/dnsblocklist/blocked_names >> blocked-names.txt

#   cat >> blocked-names.txt << EOF
# ## Add whatever other blocked domains you require here
# EOF
  popd
}

systemctl enable --now nftables.service

# configure dnscrypt and disable systemd_resolved
modify_dnscrypt_config

# trigger refresh_blocklist to populate the blocked-names.txt
if OUTPUT_DIR="${DNSCRYPT_HOME}" "$RB_SCRIPT_FILE" ; then
  # if its successful, add it to the crontab to frequently refresh the blocklist
  # the DNSCRYPT_HOME is already present as crontab env variable. refer: 91_cloud_boothook.sh
  cp "$USER_CRON_TEMPLATE" "$RB_CRON_FILE"
  cat >> "$REFRESH_BLOCKLIST_CRON_FILE" << EOF

0  *  *  *  *  root  OUTPUT_DIR="$DNSCRYPT_HOME" "$RB_SCRIPT_FILE" && systemctl restart dnscrypt-proxy.service )
EOF

else
  # refreshing blocklist failed, move on...
  echo "refresh_blocklist script failed... moving on with an empty blocked-names.txt"
  echo >> blocked-names.txt
fi

# finally disable systemd-resolved after trying the blocklist
disable_systemd_resolved

dnscrypt-proxy -service install
dnscrypt-proxy -service start
sleep 5s
dnscrypt-proxy -resolve google.com


