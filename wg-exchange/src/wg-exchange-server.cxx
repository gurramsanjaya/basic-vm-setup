#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>


using grpc::ServerUnaryReactor;
using grpc::experimental::TlsServerCredentialsOptions;
using grpc::experimental::TlsServerCredentials;
using grpc::experimental::FileWatcherCertificateProvider;
using grpc::experimental::CertificateProviderInterface;
using grpc::CallbackServerContext;
using grpc::ServerBuilder;
using grpc::Server;

ABSL_FLAG(std::string, uri, "127.0.0.1:59910", "Server Listening URI");
ABSL_FLAG(bool, tls, false, "Toggle tls on");
ABSL_FLAG(std::string, tls_private_key, "./tls/s_private.pem", "Private Key File Path");
ABSL_FLAG(std::string, tls_cert, "./tls/s_cert.pem", "Certificate Key File Path");

class WgExchangeImpl final : public WGExchange::CallbackService {
  ServerUnaryReactor* AddClient(CallbackServerContext* context, const Credentials* request, ClientConfig* response) override {
    ServerUnaryReactor* reactor = context->DefaultReactor();
    return reactor;
  }
};

// Dangling references....
void setTlsCredentials(ServerBuilder& builder, std::string& uri) {
  std::string tls_private_key = absl::GetFlag(FLAGS_tls_private_key);
  std::string tls_cert = absl::GetFlag(FLAGS_tls_cert);
  TlsServerCredentialsOptions options{ std::make_shared<FileWatcherCertificateProvider>(tls_private_key, tls_cert, 60) };
  builder.AddListeningPort(uri, TlsServerCredentials(options));
}

void setBasicCredentials(ServerBuilder& builder, std::string& uri) {
}


int main(int argc, char** argv) {
  absl::SetProgramUsageMessage("");
  absl::ParseCommandLine(argc, argv);
  std::string uri = absl::GetFlag(FLAGS_uri);
  ServerBuilder builder;
  WgExchangeImpl service;
  if(absl::GetFlag(FLAGS_tls)) {
    setTlsCredentials(builder, uri);
  } else {
    builder.AddListeningPort(uri, grpc::InsecureServerCredentials());
  }
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  server->Wait();
  return 0;
}