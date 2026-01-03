# Simple vm setup
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
    values = [var.vm-size]
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
    content = fileexists(var.cloud_config_base_template) ? templatefile(var.cloud_config_base_template, { ssh_key : file(var.ssh_public_key_file) }) : ""
    filename = "cloud_config_base"
  }
  dynamic part {
    for_each = var.cloud_config_extra
    content {
      content_type = part.value["content_type"]
      content = fileexists(part.value["content_file"]) ? file(part.value["content_file"]) : ""
      filename = part.value["filename"]
    }
  }
}

# skip if using cloud init.
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
  region = coalesce(var.vm-region-override, data.digitalocean_regions.doreg.regions[random_integer.random.result].slug)
  size = var.vm-size
  ipv6 = true
  ssh_keys = digitalocean_ssh_key.ssh_key[*].id
  resize_disk = false
  user_data = one(data.cloudinit_config.c_config[*].rendered)
}
