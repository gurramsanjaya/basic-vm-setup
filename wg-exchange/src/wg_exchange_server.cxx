#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/grpcpp.h>

#include "service_dbus_handle.h"
#include "wg_exchange.h"

using grpc::CallbackServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCredentials;
using grpc::ServerUnaryReactor;
using grpc::experimental::FileWatcherCertificateProvider;
using grpc::experimental::TlsServerCredentials;
using grpc::experimental::TlsServerCredentialsOptions;

ABSL_FLAG(std::string, uri, "127.0.0.1:59910", "Server Listening URI");
ABSL_FLAG(std::string, sys_svc_nm, "wg-quick@wgs0.service",
          "Wireguard device service");
ABSL_FLAG(bool, tls, false, "Toggle tls on");
ABSL_FLAG(std::string, tls_private_key, "./tls/s_private.pem",
          "Private Key File Path");
ABSL_FLAG(std::string, tls_cert, "./tls/s_cert.pem",
          "Certificate Key File Path");

class WGReactor : public grpc::ServerUnaryReactor {
 private:
  std::weak_ptr<ServiceDBusHandler> sys_svc_hdl_;

 public:
  WGReactor(std::weak_ptr<ServiceDBusHandler> sys_svc_hdl)
      : sys_svc_hdl_(sys_svc_hdl) {}
  void OnDone() {
    if (auto p = sys_svc_hdl_.lock()) {
      p->restart_service();
    }
  }
};

class WGExchangeImpl final : public WGExchange::CallbackService {
 private:
  std::shared_ptr<ServiceDBusHandler> sys_svc_hdl_ = nullptr;

 public:
  WGExchangeImpl(std::shared_ptr<ServiceDBusHandler> sys_svc_hdl)
      : WGExchange::CallbackService() {
    sys_svc_hdl_ = sys_svc_hdl;
  }

  GETTER(std::shared_ptr<ServiceDBusHandler>, sys_svc_hdl_)

  ServerUnaryReactor* addClient(CallbackServerContext* context,
                                const Credentials* request,
                                ClientConfig* response) override {
    WGReactor* reactor = new WGReactor(sys_svc_hdl_);
    std::cout << request->DebugString();
    reactor->Finish(grpc::Status::OK);
    return reactor;
  }
};

class IGrpcHandler {
 protected:
  ServerBuilder bldr_;
  std::string uri_;
  WGExchangeImpl svc_;
  std::unique_ptr<Server> srvr_ = nullptr;
  std::shared_ptr<ServerCredentials> creds_ = nullptr;
  IGrpcHandler(std::string uri, std::string sys_srvc_nm)
      : uri_(std::move(uri)), svc_(ServiceDBusHandler::create(sys_srvc_nm)) {}

 public:
  void init() {
    if (svc_.get_sys_svc_hdl_()->is_service()) {
      srvr_ = bldr_.BuildAndStart();
    }
  }
  void wait() {
    if (srvr_) {
      srvr_->Wait();
    }
  }
};

class GrpcHandler : public IGrpcHandler {
 public:
  GrpcHandler(std::string uri, std::string sys_svc_nm)
      : IGrpcHandler(std::move(uri), std::move(sys_svc_nm)) {
    creds_ = grpc::InsecureServerCredentials();
    bldr_.AddListeningPort(uri_, creds_);
    bldr_.RegisterService(&svc_);
  }
};

class TlsGrpcHandler : public IGrpcHandler {
 protected:
  std::string tls_priv_path_, tls_cert_path_;
  TlsServerCredentialsOptions opt_;

 public:
  TlsGrpcHandler(std::string uri, std::string sys_svc_nm,
                 std::string tls_priv_path, std::string tls_cert_path)
      : IGrpcHandler(std::move(uri), std::move(sys_svc_nm)),
        tls_priv_path_(std::move(tls_priv_path)),
        tls_cert_path_(std::move(tls_cert_path)),
        opt_(std::make_shared<FileWatcherCertificateProvider>(
            tls_priv_path_, tls_cert_path_, 60)) {
    creds_ = TlsServerCredentials(opt_);
    bldr_.AddListeningPort(uri_, creds_);
  }
};

int main(int argc, char** argv) {
  DBus::set_logging_function(DBus::log_std_err);
  DBus::set_log_level(SL_LogLevel::SL_DEBUG);
  absl::SetProgramUsageMessage("");
  absl::ParseCommandLine(argc, argv);
  std::unique_ptr<IGrpcHandler> handler = nullptr;
  if (absl::GetFlag(FLAGS_tls)) {
    handler = std::make_unique<TlsGrpcHandler>(
        absl::GetFlag(FLAGS_uri), absl::GetFlag(FLAGS_sys_svc_nm),
        absl::GetFlag(FLAGS_tls_private_key), absl::GetFlag(FLAGS_tls_cert));
  } else {
    handler = std::make_unique<GrpcHandler>(absl::GetFlag(FLAGS_uri),
                                            absl::GetFlag(FLAGS_sys_svc_nm));
  }
  handler->init();
  handler->wait();

  return 0;
}
