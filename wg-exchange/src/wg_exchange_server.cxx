#include <absl/random/random.h>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/grpcpp.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <thread>

#include "device_handle.h"
#include "wg_exchange.h"

namespace po = boost::program_options;
namespace ge = grpc::experimental;

using ge::CertificateProviderInterface;
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
    std::weak_ptr<DeviceHandler> sys_svc_hdl_;

  public:
    WGExchangeImpl() : WGExchange::CallbackService()
    {
        sys_svc_hdl_ = DeviceHandler::get_instance();
    }

    GETTER(std::weak_ptr<DeviceHandler>, sys_svc_hdl_)

    ServerUnaryReactor *addClient(CallbackServerContext *context, const Credentials *request,
                                  RequestId *response) override
    {
        ServerUnaryReactor *reactor = context->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    ServerUnaryReactor *getConfig(CallbackServerContext *context, const RequestId *request,
                                  ClientConfig *response) override
    {
        ServerUnaryReactor *reactor = context->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }
};

class GrpcHandler
{
  private:
    ServerBuilder bldr_;
    std::string url_;
    WGExchangeImpl svc_;
    std::unique_ptr<Server> srvr_ = nullptr;
    std::shared_ptr<ServerCredentials> creds_ = nullptr;
    std::shared_ptr<TlsServerCredentialsOptions> opt_ = nullptr;
    GrpcHandler() = default;

  public:
    // Insecure variation
    GrpcHandler(std::string url) : url_(std::move(url))
    {
        creds_ = grpc::InsecureServerCredentials();
        bldr_.AddListeningPort(url_, creds_);
        bldr_.RegisterService(&svc_);
    }

    // Tls variation
    GrpcHandler(std::string url, std::string tls_priv_path, std::string tls_cert_path) : url_(std::move(url))
    {
        std::shared_ptr<CertificateProviderInterface> cp =
            std::make_shared<FileWatcherCertificateProvider>(tls_priv_path, tls_cert_path, 60);
        opt_ = std::make_shared<TlsServerCredentialsOptions>(cp);
        creds_ = TlsServerCredentials(*opt_);
        bldr_.AddListeningPort(url_, creds_);
        bldr_.RegisterService(&svc_);
    }

    void init(po::variables_map vm)
    {
        if (auto p = svc_.get_sys_svc_hdl_().lock())
        {
            p->setup(vm);
            srvr_ = bldr_.BuildAndStart();
        }
    }
    void wait(const int timeout)
    {
        if (srvr_)
        {
            // TODO: Move this jury rigged timer into a separate method
            std::thread t([this, timeout]() {
                std::this_thread::sleep_for(std::chrono::minutes(timeout));
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

int main(int argc, char **argv)
{
    // DBus::set_logging_function(DBus::log_std_err);
    // DBus::set_log_level(SL_LogLevel::SL_TRACE);

    po::options_description cmd_line_desc("CmdLine Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);
    po::options_description conf_desc("Conf Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);

    // clang-format off
    conf_desc.add_options()
        ("server.url", po::value<std::string>()->default_value("127.0.0.1:59910"),"server listening endpoint")
        ("server.wg-device", po::value<std::string>()->default_value("wgs0"), "wireguard device name")
        ("server.tls", "toggle TLS on")
        ("server.tls-private-key", po::value<std::string>()->default_value("./tls/s_private.pem"), "private Key File Path")
        ("server.tls-cert", po::value<std::string>()->default_value("./tls/s_cert.pem"),"certificate file path")
        ("server.timeout", po::value<int>()->default_value(4),"timeout value after which the program will shutdown (in minutes)")
        ("Interface.Address", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.ListenPort", po::value<std::string>(), "wg-quick interface option")
        ("Interface.DNS", po::value<std::string>(), "wg-quick interface option")
        ("Interface.FwMark", po::value<std::string>(), "wg-quick interface option")
        ("Interface.PreUp", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.PostUp", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.PreDown", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.PostDown", po::value<std::vector<std::string>>(), "wg-quick interface option");

    cmd_line_desc.add_options()
        ("conf", po::value<std::string>()->default_value("./wge_server.conf"), "conf file")
        ("help", "produce help message");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmd_line_desc), vm);

    const std::string wge_conf = vm["conf"].as<std::string>();
    if (vm.count("help") || !boost::filesystem::is_regular_file(wge_conf))
    {
        std::cerr << conf_desc << '\n' << cmd_line_desc << '\n';
        return 1;
    }

    po::store(po::parse_config_file(wge_conf.c_str(), conf_desc), vm);

    if (!vm.count("server.wg-device") || vm["server.wg-device"].as<std::string>().length() == 0)
    {
        std::cerr << "Invalid wg-device";
        return 1;
    }
    // Singleton
    DeviceHandler::get_instance(vm["server.wg-device"].as<std::string>());

    std::unique_ptr<GrpcHandler> handler = nullptr;
    std::string url = vm["server.url"].as<std::string>();
    if (vm.count("server.tls"))
    {
        handler = std::make_unique<GrpcHandler>(url, vm["server.tls-private-key"].as<std::string>(),
                                                vm["server.tls-cert"].as<std::string>());
    }
    else
    {
        handler = std::make_unique<GrpcHandler>(url);
    }
    handler->init(vm);
    handler->wait(vm["timeout"].as<int>());

    return 0;
}
