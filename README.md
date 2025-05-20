# Basic vm setup
Just a basic terraform/cloud-init config for setting up a basic cloud instance

## TODO
- Remove root access for the ssh exposed user
- Add conditional azure/gcm provider instance
- Add wg credential exchange mechanism that doesn't involve cloud-init

## Notes
- Takes 1~2 minutes before the final stage cloud-init scripts get executed.
- For compiling wg-exchange, use the same compiler that you compiled gRPC with. Don't switch compilers (if gRPC was compiled with gcc then compile wg-exchange with gcc). ABI issues pop up otherwise (I think because of grpc++_reflection library)
