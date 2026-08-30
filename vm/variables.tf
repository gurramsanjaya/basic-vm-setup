variable "do_token" {
  nullable = false
  sensitive = true
}
variable "name_suffix" {
  nullable = false
}
variable "ssh_public_key_file" {
  nullable = false
  default = "../cloudinit/ssh_key.pem.pub"
}
variable "use_cloudinit" {
  type = bool
}
variable "cloud_config_base_template" {
  default = "../cloudinit/0_cloud_config.tftpl"
}

# using list here to maintain the order
variable "cloud_config_extra" {
  type = list(object({
    filename = string
    content_type = string
    content_file = string
  }))
  default = [
    # need for rendering and placing custom files
    {
      filename = "setuphandler.py"
      content_type = "text/part-handler"
      content_file = "../cloudinit/parthandler/1_setuphandler.py"
    },
    # the main nftable for the vm
    {
      filename = "nftables.conf"
      content_type = "text/x-custom-nft"
      content_file = "../cloudinit/2_nftables.conf.jinja"
    },
    # wireguard config
    {
      filename = "wg_nat.conf"
      content_type = "text/x-custom-wg"
      content_file = "../cloudinit/31_wg_nat.conf.jinja"
    },
    {
      filename = "wg_delete.conf"
      content_type = "text/x-custom-wg"
      content_file = "../cloudinit/32_wg_delete.conf"
    },
    # wireguard exchange config
    {
      filename = "server.key"
      content_type = "text/x-custom-wge"
      content_file = "../cloudinit/tls/41_server.key"
    },
    {
      filename = "server.pem"
      content_type = "text/x-custom-wge"
      content_file = "../cloudinit/tls/42_server.pem"
    },
    {
      filename = "server.toml"
      content_type = "text/x-custom-wge"
      content_file = "../cloudinit/43_server.toml.jinja"
    },
    {
      filename = "wge_nft_allow.conf"
      content_type = "text/x-custom-wge"
      content_file = "../cloudinit/44_wge_nft_allow.conf.jinja"
    },
    {
      filename = "wge_nft_delete.conf"
      content_type = "text/x-custom-wge"
      content_file = "../cloudinit/45_wge_nft_delete.conf"
    },
    # blocklist config
    {
      filename = "deny_urls"
      content_type = "text/x-custom-blocklist"
      content_file = "../cloudinit/blocklist/51_deny_urls"
    },
    {
      filename = "deny_extras"
      content_type = "text/x-custom-blocklist"
      content_file = "../cloudinit/blocklist/52_deny_extras"
    },
    {
      filename = "force_allow"
      content_type = "text/x-custom-blocklist"
      content_file = "../cloudinit/blocklist/53_force_allow"
    },
    {
      filename = "refresh_blocklist.sh"
      content_type = "text/x-custom-blocklist"
      content_file = "../cloudinit/blocklist/54_refresh_blocklist.sh"
    },
    # boothook, to be run just after the network setup phase (keep filename in ascending order of execution)
    {
      filename = "91-boothook.sh"
      content_type = "text/cloud-boothook"
      content_file = "../cloudinit/91_cloud_boothook_setup.sh"
    },
    # posthooks, to be run after all the cloudinit module configs (skip file name so that they are run in order)
    {
      filename = "92-posthook.sh"
      content_type = "text/x-shellscript"
      content_file = "../cloudinit/92_cloud_posthook_dnscrypt.sh"
    },
    {
      filename = "93-posthook.sh"
      content_type = "text/x-shellscript"
      content_file = "../cloudinit/93_cloud_posthook_wge.sh"
    },
    {
      filename = "94-posthook.sh"
      content_type = "text/cloud-boothook"
      content_file = "../cloudinit/91_cloud_posthook_machineid_issue_fix.sh"
    },
  ]
  nullable = true
  description = "list of cloudinit files that will be archived together in multipart mime format and sent"
}
variable "vm-size" {
  nullable = false
  type = string
  default = "s-1vcpu-512mb-10gb"
}
variable "vm-region-override" {
  nullable = true
  type = string
}
