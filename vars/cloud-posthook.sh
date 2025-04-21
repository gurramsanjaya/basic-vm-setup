#!/bin/bash

function disable_systemd_resolved() {
    # TODO: Need to make this atomic using traps
    systemctl disable --now systemd-resolved.service
    cat > /etc/resolv.conf << EOF
nameserver 127.0.0.1
options edns0 trust-ad
search .
EOF
}
    
systemctl enable --now nftables.service

disable_systemd_resolved
dnscrypt-proxy -service install
dnscrypt-proxy -service start
sleep 5s
dnscrypt-proxy -resolve google.com