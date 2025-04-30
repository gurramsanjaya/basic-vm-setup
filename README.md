# Basic vm setup
Just a basic terraform/cloud-init config for setting up a basic cloud instance

## TODO
- Remove root access for the ssh exposed user
- Add conditional azure/gcm provider instance
- Add wg credential exchange mechanism that doesn't involve cloud-init

## Notes
Takes 1~2 minutes before the final stage cloud-init scripts get executed.
