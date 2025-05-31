#include <boost/program_options.hpp>
#include <absl/random/random.h>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/grpcpp.h>

#include "ext_hacl_star/Hacl_Curve25519_51.h"
#include "service_dbus_handle.h"
#include "wg_exchange.h"

namespace po = boost::program_options;
namespace ge = grpc::experimental;

using grpc::CallbackServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCredentials;
using grpc::ServerUnaryReactor;
using ge::FileWatcherCertificateProvider;
using ge::TlsServerCredentials;
using ge::TlsServerCredentialsOptions;

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

    // absl::BitGen gen;
    // uint8_t* key = new uint8_t[WG_KEY_LEN];
    // for (int i = 0; i < WG_KEY_LEN / sizeof(uint64_t); i++) {
    //   uint64_t temp = gen();
    //   std::memcpy(key + i * 8, &temp, sizeof(uint64_t));
    // }
    // char* bas64_key = new char[WG_KEY_LEN_BASE64];
    // key_to_base64(bas64_key, key);
    // std::cout << bas64_key << '\n';
    // delete[] bas64_key;

    // uint8_t* pub_key = new uint8_t[WG_KEY_LEN];
    // Hacl_Curve25519_51_secret_to_public(pub_key, key);
    // std::string str_pub_key{reinterpret_cast<char(*)>(pub_key), WG_KEY_LEN};

    // delete[] key;
    // response->mutable_peer()->mutable_creds()->set_pub_key(str_pub_key);

    reactor->Finish(grpc::Status::OK);
    return reactor;
  }
};

class IGrpcHandler {
 protected:
  ServerBuilder bldr_;
  std::string url_;
  WGExchangeImpl svc_;
  std::unique_ptr<Server> srvr_ = nullptr;
  std::shared_ptr<ServerCredentials> creds_ = nullptr;
  IGrpcHandler() = delete;
  IGrpcHandler(std::string url, std::string sys_srvc_nm)
      : url_(std::move(url)), svc_(ServiceDBusHandler::create(sys_srvc_nm)) {}

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
  GrpcHandler(std::string url, std::string sys_svc_nm)
      : IGrpcHandler(std::move(url), std::move(sys_svc_nm)) {
    creds_ = grpc::InsecureServerCredentials();
    bldr_.AddListeningPort(url_, creds_);
    bldr_.RegisterService(&svc_);
  }
};

class TlsGrpcHandler : public IGrpcHandler {
 protected:
  std::string tls_priv_path_, tls_cert_path_;
  TlsServerCredentialsOptions opt_;

 public:
  TlsGrpcHandler(std::string url, std::string sys_svc_nm,
                 std::string tls_priv_path, std::string tls_cert_path)
      : IGrpcHandler(std::move(url), std::move(sys_svc_nm)),
        tls_priv_path_(std::move(tls_priv_path)),
        tls_cert_path_(std::move(tls_cert_path)),
        opt_(std::make_shared<FileWatcherCertificateProvider>(
            tls_priv_path_, tls_cert_path_, 60)) {
    creds_ = TlsServerCredentials(opt_);
    bldr_.AddListeningPort(url_, creds_);
    bldr_.RegisterService(&svc_);
  }
};

int main(int argc, char** argv) {
  // DBus::set_logging_function(DBus::log_std_err);
  // DBus::set_log_level(SL_LogLevel::SL_DEBUG);

  po::options_description desc("Allowed Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);
  desc.add_options()
    ("help", "produce help message")
    ("url", po::value<std::string>()->default_value("127.0.0.1:59910"), "server listening endpoint")
    ("system-service-name", po::value<std::string>()->default_value("wg-quick@wgs0.service"), "wireguard device service")
    ("tls","toggle TLS on")
    ("tls-private-key", po::value<std::string>()->default_value("./tls/s_private.pem"),"private Key File Path")
    ("tls-cert", po::value<std::string>()->default_value("./tls/s_cert.pem"), "certificate file path");
  po::variables_map vm;
  po::store(po::parse_command_line(argc,argv,desc), vm);
  if(vm.count("help")) {
    std::cout << desc << '\n';
    return 1;
  }
  std::unique_ptr<IGrpcHandler> handler = nullptr;
  const std::string url = vm["url"].as<std::string>();
  const std::string sys_svc_nm = vm["system-service-name"].as<std::string>();
  if (vm.count("tls")) {
    handler = std::make_unique<TlsGrpcHandler>(
        url, sys_svc_nm,
        vm["tls-private-key"].as<std::string>(), vm["tls-cert"].as<std::string>());
  } else {
    handler = std::make_unique<GrpcHandler>(url,
                                            sys_svc_nm);
  }
  handler->init();
  handler->wait();

  return 0;
}
