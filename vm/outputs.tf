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