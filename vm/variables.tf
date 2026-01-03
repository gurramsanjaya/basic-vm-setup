variable "do_token" {
  nullable = false
  sensitive = true
}
variable "name_suffix" {
  nullable = false
}
variable "ssh_public_key_file" {
  nullable = false
  default = "../vars/ssh_key.pem.pub"
}
variable "use_cloudinit" {
  type = bool
}
variable "cloud_config_base_template" {
  default = "../vars/0_cloud_config.tftpl"
}

# using list here to maintain the order
variable "cloud_config_extra" {
  type = list(object({
    filename = string
    content_type = string
    content_file = string
  }))
  default = [
    {
      filename = "setuphandler.py"
      content_type = "text/part-handler"
      content_file = "../parthandler/1_setuphandler.py"
    },
    {
      filename = "nftables.conf"
      content_type = "text/x-custom-nft"
      content_file = "../vars/2_nftables.conf.jinja"
    },
    {
      filename = "wg_nat.conf"
      content_type = "text/x-custom-wg"
      content_file = "../vars/31_wg_nat.conf.jinja"
    },
    {
      filename = "wg_delete.conf"
      content_type = "text/x-custom-wg"
      content_file = "../vars/32_wg_delete.conf"
    },
    {
      filename = "server.key"
      content_type = "text/x-custom-wge"
      content_file = "../tls/41_server.key"
    },
    {
      filename = "server.pem"
      content_type = "text/x-custom-wge"
      content_file = "../tls/42_server.pem"
    },
    {
      filename = "server.toml"
      content_type = "text/x-custom-wge"
      content_file = "../vars/43_server.toml.jinja"
    },
    {
      filename = "wge_nft_allow.conf"
      content_type = "text/x-custom-wge"
      content_file = "../vars/44_wge_nft_allow.conf.jinja"
    },
    {
      filename = "wge_nft_delete.conf"
      content_type = "text/x-custom-wge"
      content_file = "../vars/45_wge_nft_delete.conf"
    },
    {
      filename = "cloud_boothook.sh"
      content_type = "text/cloud-boothook"
      content_file = "../vars/91_cloud_boothook.sh"
    },
    {
      filename = "cloud_posthook_dnscrypt.sh"
      content_type = "text/x-shellscript"
      content_file = "../vars/92_cloud_posthook_dnscrypt.sh"
    },
    {
      filename = "cloud_posthook_wge.sh"
      content_type = "text/x-shellscript"
      content_file = "../vars/93_cloud_posthook_wge.sh"
    }
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
