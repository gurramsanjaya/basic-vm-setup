#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/create_channel.h>

using grpc::Channel;
using grpc::ChannelCredentials;
using grpc::CreateChannel;
using grpc::experimental::CertificateProviderInterface;
using grpc::experimental::FileWatcherCertificateProvider;
using grpc::experimental::TlsChannelCredentialsOptions;
using grpc::experimental::TlsCredentials;

ABSL_FLAG(std::string, target, "127.0.0.1:59910", "Server exchange endpoint");
ABSL_FLAG(bool, tls, false, "Toggle tls on");
ABSL_FLAG(std::string, tls_cert, "./tls/c_cert.pem", "Root Cert File Path");

class ClientHandler {
 protected:
  std::string tgt_;
  std::shared_ptr<Channel> chan_ = nullptr;
  std::shared_ptr<ChannelCredentials> creds_ = nullptr;
  std::unique_ptr<WGExchange::Stub> stub_ = nullptr;
  ClientHandler() {}

 public:
  ClientHandler(std::string tgt)
      : tgt_(std::move(tgt)), creds_(grpc::InsecureChannelCredentials()) {
    chan_ = CreateChannel(tgt_, creds_);
    stub_ = WGExchange::NewStub(chan_);
  }
  void addClient() {
    grpc::ClientContext context;
    Credentials request;
    ClientConfig response;
    stub_->addClient(&context, request, &response);
    if (response.has_peer()) {
      if (response.peer().has_creds() > 0) {
        std::cout << response.peer().creds().pub_key() << '\n';
      }
    }
  }
};

class TlsClientHandler : public ClientHandler {
 protected:
  std::string tls_cert_path_;
  std::shared_ptr<CertificateProviderInterface> cert_prov_ = nullptr;
  TlsChannelCredentialsOptions opt_;

 public:
  TlsClientHandler(std::string tgt, std::string tls_cert_path)
      : tls_cert_path_(std::move(tls_cert_path)) {
    tgt_ = std::move(tgt);
    cert_prov_ =
        std::make_shared<FileWatcherCertificateProvider>(tls_cert_path_, 60);
    opt_.set_certificate_provider(cert_prov_);
    creds_ = TlsCredentials(opt_);
    chan_ = CreateChannel(tgt_, creds_);
    stub_ = WGExchange::NewStub(chan_);
  }
};

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage("");
  absl::ParseCommandLine(argc, argv);
  std::string target = absl::GetFlag(FLAGS_target);
  std::unique_ptr<ClientHandler> handler = nullptr;
  if (absl::GetFlag(FLAGS_tls)) {
    handler = std::make_unique<TlsClientHandler>(target,
                                                 absl::GetFlag(FLAGS_tls_cert));
  } else {
    handler = std::make_unique<ClientHandler>(target);
  }
  handler->addClient();
  return 0;
}
