# Simple vm setup
terraform {
  required_providers {
    digitalocean = {
      source  = "digitalocean/digitalocean"
      version = "~> 2.0"
    }
    random = {
      source = "hashicorp/random"
      version = "3.7.1"
    }
    cloudinit = {
      source = "hashicorp/cloudinit"
      version = "2.3.6"
    }
  }
}

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

provider "digitalocean" {
    token = var.do_token
}

data "digitalocean_regions" "doreg" {
    filter {
        key = "available"
        values = ["true"]
    }
    filter {
        key = "sizes"
        values = ["s-1vcpu-512mb-10gb"]
    }
}

# Should output MIME multi-part archive not compressed with gzip, cloud-init throws error on base64 encoded mime content
data "cloudinit_config" "c_config" {
  count = var.use_cloudinit ? 1 : 0
  # Can't base64 encode the mime multipart config - https://github.com/canonical/cloud-init/issues/4239
  base64_encode = false
  gzip = false
  part {
    content_type = "text/cloud-config"
    content = fileexists(var.cloud_config_base_template) ? templatefile(var.cloud_config_base_template, { ssh_key : file(var.ssh_public_key_file) }) : null
    filename = "cloud_config_base"
  }
  dynamic part {
    for_each = var.cloud_config_extra
    content {
      content_type = part.value["content_type"]
      content = fileexists(part.value["file"]) ? file(part.value["file"]) : null
      filename = part.key
    }
  }
}

resource "digitalocean_ssh_key" "ssh_key" {
    count = var.use_cloudinit ? 0 : 1
    name = "ssh-${var.name_suffix}"
    public_key = file(var.ssh_public_key_file)
}

# For randomizing the region
resource "random_integer" "random" {
    min = 0
    max = length(data.digitalocean_regions.doreg.regions) - 1
}

resource "digitalocean_droplet" "web" {
    image = "ubuntu-22-04-x64"
    name = "vm-${var.name_suffix}"
    region = data.digitalocean_regions.doreg.regions[random_integer.random.result].slug
    size = "s-1vcpu-512mb-10gb"
    ipv6 = true
    ssh_keys = digitalocean_ssh_key.ssh_key[*].id
    resize_disk = false
    user_data = one(data.cloudinit_config.c_config[*].rendered)
}

output "ipv4_address" {
  value = digitalocean_droplet.web.ipv4_address
}
output "ipv6_address" {
  value = digitalocean_droplet.web.ipv6_address
}
output "region" {
  value = digitalocean_droplet.web.region
}
output "size" {
  value = digitalocean_droplet.web.size
}