# Cloud-init assets

## Structure overview
- `0_cloud_config.tftpl` delivers the base cloud-config that installs packages such as WireGuard, creates the `test` user, and is exposed to Terraform as the `cloud_config_base_template` entry in [`vm/variables.tf`](../vm/variables.tf#L27).
- `parthandler/` contains the Cloud-init part handler (`setuphandler.py`), which renders templated payloads with runtime `default_interface`/`external_ip` values before writing files to the VM (see [`cloudinit/parthandler/1_setuphandler.py`](cloudinit/parthandler/1_setuphandler.py)).
- Static and templated assets (nftables, WireGuard/WGE configs, blocklists, TLS credentials, hook scripts) live beside their target directories so Terraform can enumerate and publish them in order via `cloud_config_extra`.
- Supporting data such as `blocklist/`, `tls/`, and the ignored `ssh_key.pem(.pub)` pair round out the files archived into the droplet.

## Cloud-init packaging and ordering
- Terraform’s `cloud_config_extra` list defines each MIME part by `filename`, `content_type`, and `content_file`, so files are archived into cloud-init sequentially before the resulting MIME bundle is provided to `digitalocean_droplet.user_data` ([vm/main.tf](../vm/main.tf#L17)).
- The handler maps the `text/x-custom-*` content types to the appropriate directories (`/etc/`, `/etc/wge/`, `/etc/wireguard/`, `/etc/blocklist/`, `/etc/crontab/`), and uses the digit-prefixed filenames to keep track of what executes when (`91-` for boothooks, `92-/93-/94-` for posthooks).
- `91-*-cloud`/`91-*.sh` boothooks run immediately after the networking phase, preparing binaries and variables, while the `92-`, `93-`, and final `94-` posthooks run after all internal cloud-init modules finish ([`cloudinit/91_cloud_boothook_setup.sh`](cloudinit/91_cloud_boothook_setup.sh), [`cloudinit/92_cloud_posthook_dnscrypt.sh`](cloudinit/92_cloud_posthook_dnscrypt.sh), [`cloudinit/93_cloud_posthook_wge.sh`](cloudinit/93_cloud_posthook_wge.sh), [`cloudinit/94_cloud_posthook_machineid_issue_fix.sh`](cloudinit/94_cloud_posthook_machineid_issue_fix.sh)).
- This naming and ordering keeps boot tasks deterministic: base config first, extras in order, hooks triggered by their prefixes, and the handler ensuring files land in place for each phase.

