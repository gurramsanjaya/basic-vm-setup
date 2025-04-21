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
  default = "../vars/cloud-config.tftpl"
}
variable "cloud_config_extra" {
  type = map(object({
    content_type = string
    file = string
  }))
  default = {
    "cloud-boothook.sh" = {
      content_type = "text/cloud-boothook"
      file = "../vars/cloud-boothook.sh"
    },
    "cloud-posthook.sh" = {
      content_type = "text/x-shellscript"
      file = "../vars/cloud-posthook.sh"
    }
  }
  nullable = true
}