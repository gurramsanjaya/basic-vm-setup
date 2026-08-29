#!/bin/bash

# This seems to break the other scripts if it goes in either in boothook or anything except the last posthook (not completely sure why)
# hence, keeping this in the very last posthook script

function fix_machineid_journalctl_issue() {
  # machine-id gets renewed by the vender (digitalocean) script, but the journal daemon is still using the old machine id
  # therefore any logs till now is still getting logged into the old machineid folder and will not be accessible
  # if we simply restart journald to make it pick the new machine-id, we need to move the old logs to the new machineid folder
  local old_machine_id=$(journalctl -n 1 --output=json | jq -r ._MACHINE_ID )
  local new_machine_id=$(cat /etc/machine-id)

  # moving contents of the old to the new one
  systemctl stop systemd-journald
  # cp -a "/var/log/journal/${old_machine_id}/" "/var/log/journal/${new_machine_id}/"
  mv "/var/log/journal/${old_machine_id}/" "/var/log/journal/${new_machine_id}/"
  chown -R root:systemd-journal "/var/log/journal/${new_machine_id}/"
  systemctl start systemd-journald
}

fix_machineid_journalctl_issue