#include <generated/wg_exchange.grpc.pb.h>
#include <generated/wg_exchange.pb.h>
#include <grpcpp/create_channel.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>

using grpc::ChannelCredentials;
using grpc::Channel;
using grpc::CreateChannel;
using grpc::experimental::TlsCredentials;
using grpc::experimental::TlsChannelCredentialsOptions;
using grpc::experimental::FileWatcherCertificateProvider;

ABSL_FLAG(std::string, target, "127.0.0.1:59910", "Server exchange endpoint");
ABSL_FLAG(bool, tls, false, "Toggle tls on");
ABSL_FLAG(std::string, tls_private_key, "./tls/c_private.pem", "Private Key File Path");
ABSL_FLAG(std::string, tls_cert, "./tls/c_cert.pem", "Certificate Key File Path");

class WGExchangeClient {
  public:
    WGExchangeClient(std::shared_ptr<Channel> channel): stub_(WGExchange::NewStub(channel)) {

    }
  private:
    std::unique_ptr<WGExchange::Stub> stub_;
};

// Dangling references....
std::shared_ptr<ChannelCredentials> getTlsChannelCredentials() {
  std::string tls_private_key = absl::GetFlag(FLAGS_tls_private_key);
  std::string tls_cert = absl::GetFlag(FLAGS_tls_cert);
  TlsChannelCredentialsOptions options;
  options.set_certificate_provider(std::make_shared<FileWatcherCertificateProvider>(tls_private_key, tls_cert, 60));
  return grpc::experimental::TlsCredentials(options);
}


int main(int argc, char** argv) {
  absl::SetProgramUsageMessage("");
  absl::ParseCommandLine(argc, argv);
  std::string target = absl::GetFlag(FLAGS_target);
  std::shared_ptr<ChannelCredentials> creds = nullptr;
  if(absl::GetFlag(FLAGS_tls)) {
    creds = getTlsChannelCredentials();
  } else {
    creds = grpc::InsecureChannelCredentials();
  }
  WGExchangeClient client{ CreateChannel(target, creds) };

  return 0;
}
