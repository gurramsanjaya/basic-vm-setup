#include <absl/random/random.h>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/grpcpp.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "ext_hacl_star/Hacl_Curve25519_51.h"
#include "service_dbus_handle.h"
#include "wg_exchange.h"

namespace po = boost::program_options;
namespace ge = grpc::experimental;

using ge::FileWatcherCertificateProvider;
using ge::TlsServerCredentials;
using ge::TlsServerCredentialsOptions;
using grpc::CallbackServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCredentials;
using grpc::ServerUnaryReactor;


class WGExchangeImpl final : public WGExchange::CallbackService
{
  private:
    std::weak_ptr<ServiceDBusHandler> sys_svc_hdl_;

  public:
    WGExchangeImpl(std::weak_ptr<ServiceDBusHandler> sys_svc_hdl) : WGExchange::CallbackService()
    {
        sys_svc_hdl_ = sys_svc_hdl;
    }

    GETTER(std::weak_ptr<ServiceDBusHandler>, sys_svc_hdl_)

    ServerUnaryReactor *addClient(CallbackServerContext *context, const Credentials *request,
                                  RequestId *response) override
    {
        ServerUnaryReactor* reactor = context->DefaultReactor();
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

    ServerUnaryReactor *getConfig(CallbackServerContext *context, const RequestId *request, ClientConfig *response) override {
        ServerUnaryReactor* reactor = context->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }
};

// TODO: Refactor the badly written inheritence into composition
class IGrpcHandler
{
  protected:
    ServerBuilder bldr_;
    std::string url_;
    WGExchangeImpl svc_;
    std::unique_ptr<Server> srvr_ = nullptr;
    std::shared_ptr<ServerCredentials> creds_ = nullptr;
    IGrpcHandler() = delete;
    IGrpcHandler(std::string url, std::string sys_srvc_nm)
        : url_(std::move(url)), svc_(ServiceDBusHandler::get_instance(sys_srvc_nm))
    {
    }

  public:
    void init()
    {
        if (auto p = svc_.get_sys_svc_hdl_().lock())
        {
            p->is_service();
            srvr_ = bldr_.BuildAndStart();
        }
    }
    void wait(const int timeout)
    {
        if (srvr_)
        {
            // TODO: Move this jury rigged timer into a separate method
            boost::thread t([this, timeout]() {
                boost::this_thread::sleep_for(boost::chrono::minutes(timeout));
                if (this->srvr_)
                {
                    this->srvr_->Shutdown();
                    if (auto p = this->svc_.get_sys_svc_hdl_().lock())
                    {
                        p->forget_instance();
                    }
                }
            });
            srvr_->Wait();
            if (t.joinable())
            {
                t.join();
            }
        }
    }
};

class GrpcHandler : public IGrpcHandler
{
  public:
    GrpcHandler(std::string url, std::string sys_svc_nm) : IGrpcHandler(std::move(url), std::move(sys_svc_nm))
    {
        creds_ = grpc::InsecureServerCredentials();
        bldr_.AddListeningPort(url_, creds_);
        bldr_.RegisterService(&svc_);
    }
};

class TlsGrpcHandler : public IGrpcHandler
{
  protected:
    std::string tls_priv_path_, tls_cert_path_;
    TlsServerCredentialsOptions opt_;

  public:
    TlsGrpcHandler(std::string url, std::string sys_svc_nm, std::string tls_priv_path, std::string tls_cert_path)
        : IGrpcHandler(std::move(url), std::move(sys_svc_nm)), tls_priv_path_(std::move(tls_priv_path)),
          tls_cert_path_(std::move(tls_cert_path)),
          opt_(std::make_shared<FileWatcherCertificateProvider>(tls_priv_path_, tls_cert_path_, 60))
    {
        creds_ = TlsServerCredentials(opt_);
        bldr_.AddListeningPort(url_, creds_);
        bldr_.RegisterService(&svc_);
    }
};

int main(int argc, char **argv)
{
    // DBus::set_logging_function(DBus::log_std_err);
    // DBus::set_log_level(SL_LogLevel::SL_TRACE);

    po::options_description cmd_line_desc("CmdLine Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);
    po::options_description conf_desc("Conf Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);

    conf_desc.add_options()
        ("server.url", po::value<std::string>()->default_value("127.0.0.1:59910"),"server listening endpoint")
        ("server.wg-conf-file", po::value<std::string>()->default_value("wgs0.conf"), "wireguard device conf file in /etc/wireguard/")
        ("server.tls", "toggle TLS on")
        ("server.tls-private-key", po::value<std::string>()->default_value("./tls/s_private.pem"), "private Key File Path")
        ("server.tls-cert", po::value<std::string>()->default_value("./tls/s_cert.pem"),"certificate file path")
        ("server.timeout", po::value<int>()->default_value(4),"timeout value after which the program will shutdown (in minutes)");

    cmd_line_desc.add_options()
        ("conf", po::value<std::string>()->default_value("./wge_server.conf"), "conf file")
        ("help", "produce help message");
    cmd_line_desc.add(conf_desc);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmd_line_desc), vm);

    const std::string wge_conf = vm["conf"].as<std::string>();
    if(vm.count("help") || !boost::filesystem::is_regular_file(wge_conf))
    {
        std::cerr << conf_desc << '\n' << cmd_line_desc << '\n';
        return 1;
    }

    po::store(po::parse_config_file(wge_conf.c_str(), conf_desc), vm);

    const std::string url = vm["url"].as<std::string>();
    const std::string wg_conf_file = vm["wg-conf-file"].as<std::string>();
    std::size_t pos = wg_conf_file.rfind(".conf");

    if (pos == wg_conf_file.npos || !boost::filesystem::is_regular_file("/etc/wireguard/" + wg_conf_file))
    {
        std::cerr << cmd_line_desc << '\n';
        return 1;
    };

    const std::string sys_svc_nm{"wg-quick@" + wg_conf_file.substr(0, pos) + ".service"};
    std::unique_ptr<IGrpcHandler> handler = nullptr;
    if (vm.count("tls"))
    {
        handler = std::make_unique<TlsGrpcHandler>(url, sys_svc_nm, vm["tls-private-key"].as<std::string>(),
                                                   vm["tls-cert"].as<std::string>());
    }
    else
    {
        handler = std::make_unique<GrpcHandler>(url, sys_svc_nm);
    }
    handler->init();
    handler->wait(vm["timeout"].as<int>());

    return 0;
}
