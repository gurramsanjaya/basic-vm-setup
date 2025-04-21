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