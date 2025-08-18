#include <boost/program_options.hpp>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/create_channel.h>

#include "conf_update.h"
#include "wg_exchange.h"

namespace po = boost::program_options;
namespace ge = grpc::experimental;

using ge::CertificateProviderInterface;
using ge::FileWatcherCertificateProvider;
using ge::TlsChannelCredentialsOptions;
using ge::TlsCredentials;
using grpc::Channel;
using grpc::ChannelCredentials;
using grpc::CreateChannel;

class ClientHandler
{
  protected:
    std::string tgt_;
    std::shared_ptr<Channel> chan_ = nullptr;
    std::shared_ptr<ChannelCredentials> creds_ = nullptr;
    std::unique_ptr<WGExchange::Stub> stub_ = nullptr;
    std::shared_ptr<TlsChannelCredentialsOptions> opt_ = nullptr;
    ConfUpdater conf_updater_;

    ClientHandler(std::string &&device_nm) : conf_updater_(device_nm + ".conf")
    {
    }

  public:
    // Insecure variation
    ClientHandler(std::string device_nm, std::string tgt) : ClientHandler(std::move(device_nm))
    {
        tgt_ = tgt;
        creds_ = grpc::InsecureChannelCredentials();
        chan_ = CreateChannel(tgt_, creds_);
        stub_ = WGExchange::NewStub(chan_);
    }

    // Tls variation
    ClientHandler(std::string device_nm, std::string tgt, std::string tls_cert_path)
        : ClientHandler(std::move(device_nm))
    {
        tgt_ = tgt;
        opt_ = std::make_shared<TlsChannelCredentialsOptions>();
        opt_->set_certificate_provider(std::make_shared<FileWatcherCertificateProvider>(tls_cert_path, 60));
        creds_ = TlsCredentials(*opt_);
        chan_ = CreateChannel(tgt_, creds_);
        stub_ = WGExchange::NewStub(chan_);
    }

    void addClient()
    {
        grpc::ClientContext context;
        Credentials request;
        RequestId response;
        stub_->addClient(&context, request, &response);
        if (response.uuid().length() > 0)
        {
            std::cerr << response.uuid();
        }
    }
};

int main(int argc, char **argv)
{
    po::options_description cmd_line_desc("CmdLine Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);
    po::options_description conf_desc("Conf Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);

    // clang-format off
    conf_desc.add_options()
        ("client.target", po::value<std::string>()->default_value("127.0.0.1:59910"), "target exchange endpoint")
        ("client.wg-device", po::value<std::string>()->default_value("wgc0"), "wireguard device name")
        ("client.tls", "toggle TLS on")
        ("client.tls-cert", po::value<std::string>()->default_value("./tls/c_cert.pem"), "root cert file path")
        ("Interface.DNS", po::value<std::string>(), "wg-quick interface option (will be overwritten if server sends any)")
        ("Interface.FwMark", po::value<std::string>(), "wg-quick interface option")
        ("Interface.PreUp", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.PostUp", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.PreDown", po::value<std::vector<std::string>>(), "wg-quick interface option")
        ("Interface.PostDown", po::value<std::vector<std::string>>(), "wg-quick interface option");

    cmd_line_desc.add_options()
        ("help", "produce help message")
        ("conf", po::value<std::string>()->default_value("./wge_server.conf"), "conf file");

    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmd_line_desc), vm);

    std::string wge_conf = vm["conf"].as<std::string>();
    if (vm.count("help") || !boost::filesystem::is_regular_file(wge_conf))
    {
        std::cout << cmd_line_desc << '\n';
        return 1;
    }

    po::store(po::parse_config_file(wge_conf.c_str(), conf_desc), vm);

    std::unique_ptr<ClientHandler> handler = nullptr;
    std::string target = vm["client.target"].as<std::string>();
    std::string device_nm = vm["client.wg-device"].as<std::string>();
    if (vm.count("client.tls"))
    {
        handler = std::make_unique<ClientHandler>(device_nm, target, vm["client.tls-cert"].as<std::string>());
    }
    else
    {
        handler = std::make_unique<ClientHandler>(device_nm, target);
    }
    handler->addClient();
    return 0;
}
