#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>


using grpc::ServerUnaryReactor;
using grpc::ServerCredentials;
using grpc::experimental::TlsServerCredentialsOptions;
using grpc::experimental::TlsServerCredentials;
using grpc::experimental::FileWatcherCertificateProvider;
using grpc::CallbackServerContext;
using grpc::ServerBuilder;
using grpc::Server;

ABSL_FLAG(std::string, uri, "127.0.0.1:59910", "Server Listening URI");
ABSL_FLAG(bool, tls, false, "Toggle tls on");
ABSL_FLAG(std::string, tls_private_key, "./tls/s_private.pem", "Private Key File Path");
ABSL_FLAG(std::string, tls_cert, "./tls/s_cert.pem", "Certificate Key File Path");

class WGExchangeImpl final : public WGExchange::CallbackService {
  ServerUnaryReactor* addClient(CallbackServerContext* context, const Credentials* request, ClientConfig* response) override {
    ServerUnaryReactor* reactor = context->DefaultReactor();
    std::cout << request->DebugString();
    reactor->Finish(grpc::Status::OK);
    return reactor;
  }
};

class ServerHandler {
  protected:
    std::string uri_;
    ServerBuilder bldr_;
    WGExchangeImpl srvc_;
    std::shared_ptr<ServerCredentials> creds_ = nullptr;
    std::unique_ptr<Server> srv_ = nullptr;
    ServerHandler() {}
  public:
    ServerHandler(std::string uri): uri_(std::move(uri)), creds_(grpc::InsecureServerCredentials()) {
      bldr_.AddListeningPort(uri_, creds_);
      bldr_.RegisterService(&srvc_);
    }
    void init() {
      srv_ = bldr_.BuildAndStart();
    }
    void wait() {
      srv_->Wait();
    }
};

class TlsServerHandler: public ServerHandler {
  protected:
    std::string tls_priv_path_, tls_cert_path_;
    TlsServerCredentialsOptions opt_;

  public:
    TlsServerHandler(std::string uri, std::string tls_priv_path, std::string tls_cert_path): tls_priv_path_(std::move(tls_priv_path)), tls_cert_path_(std::move(tls_cert_path)), opt_(std::make_shared<FileWatcherCertificateProvider>(tls_priv_path_, tls_cert_path_, 60)) {
      uri_ = std::move(uri);
      creds_ = TlsServerCredentials(opt_);
      bldr_.AddListeningPort(uri_, creds_);
    }
};

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage("");
  absl::ParseCommandLine(argc, argv);
  std::string uri = absl::GetFlag(FLAGS_uri);
  std::unique_ptr<ServerHandler> handler = nullptr;
  if(absl::GetFlag(FLAGS_tls)) {
    handler = std::make_unique<TlsServerHandler>(uri, absl::GetFlag(FLAGS_tls_private_key), absl::GetFlag(FLAGS_tls_cert));
  } else {
    handler = std::make_unique<ServerHandler>(uri);
  }
  handler->init();
  std::cout << "Started" << std::endl;
  handler->wait();
  return 0;
}
