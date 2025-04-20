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
  }
}

variable "do_token" {}
variable "name_suffix" {}
variable "disable_ssh" {
  type = bool
}
variable "ssh_public_key_file" {
  default = "../vars/ssh_key.pem.pub"
}
variable "user_data_file" {
  default = "../vars/user-script-file.yaml"
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

resource "random_integer" "random" {
    min = 0
    max = length(data.digitalocean_regions.doreg.regions) - 1
}

resource "digitalocean_ssh_key" "ssh_key" {
    count = var.disable_ssh ? 0 : 1
    name = "ssh-${var.name_suffix}"
    public_key = file(var.ssh_public_key_file)
}

resource "digitalocean_droplet" "web" {
    image = "ubuntu-22-04-x64"
    name = "vm-${var.name_suffix}"
    region = data.digitalocean_regions.doreg.regions[random_integer.random.result].slug
    size = "s-1vcpu-512mb-10gb"
    ipv6 = true
    ssh_keys = digitalocean_ssh_key.ssh_key[*].id
    resize_disk = false
    user_data = fileexists(var.user_data_file) ? file(var.user_data_file) : null
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