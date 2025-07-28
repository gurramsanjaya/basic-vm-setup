#include <boost/program_options.hpp>
#include <generated/wg_exchange.grpc.pb.h>
#include <grpcpp/create_channel.h>

#include "wg_exchange.h"
#include "conf_update.h"

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
    ClientHandler()
    {
    }

  public:
    ClientHandler(std::string tgt) : tgt_(std::move(tgt)), creds_(grpc::InsecureChannelCredentials())
    {
        chan_ = CreateChannel(tgt_, creds_);
        stub_ = WGExchange::NewStub(chan_);
    }
    void addClient()
    {
        grpc::ClientContext context;
        Credentials request;
        RequestId response;
        stub_->addClient(&context, request, &response);
        if(response.uuid().length() > 0) {
          std::cerr << response.uuid();
        }
                // if(str_pub_key.length() == WG_KEY_LEN) {
                //   const uint8_t* pub_key = reinterpret_cast<const uint8_t(*)>(str_pub_key.data());
                //   char* base64_pub_key = new char[WG_KEY_LEN_BASE64];
                //   key_to_base64(base64_pub_key, pub_key);
                //   std::cout << base64_pub_key << '\n';
                //   delete[] base64_pub_key;
                // }
    }
};

class TlsClientHandler : public ClientHandler
{
  protected:
    std::string tls_cert_path_;
    std::shared_ptr<CertificateProviderInterface> cert_prov_ = nullptr;
    TlsChannelCredentialsOptions opt_;

  public:
    TlsClientHandler(std::string tgt, std::string tls_cert_path) : tls_cert_path_(std::move(tls_cert_path))
    {
        tgt_ = std::move(tgt);
        cert_prov_ = std::make_shared<FileWatcherCertificateProvider>(tls_cert_path_, 60);
        opt_.set_certificate_provider(cert_prov_);
        creds_ = TlsCredentials(opt_);
        chan_ = CreateChannel(tgt_, creds_);
        stub_ = WGExchange::NewStub(chan_);
    }
};

int main(int argc, char **argv)
{
    po::options_description desc("Allowed Options", DEFAULT_LINE_LENGTH, DEFAULT_DESCRIPTION_LENGTH);
    desc.add_options()("help", "produce help message")("target",
                                                       po::value<std::string>()->default_value("127.0.0.1:59910"),
                                                       "server exchange endpoint")("tls", "toggle TLS on")(
        "tls-cert", po::value<std::string>()->default_value("./tls/c_cert.pem"), "root cert file path");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help"))
    {
        std::cout << desc << '\n';
        return 1;
    }
    const std::string target = vm["target"].as<std::string>();
    std::unique_ptr<ClientHandler> handler = nullptr;
    if (vm.count("tls"))
    {
        handler = std::make_unique<TlsClientHandler>(target, vm["tls-cert"].as<std::string>());
    }
    else
    {
        handler = std::make_unique<ClientHandler>(target);
    }
    handler->addClient();
    return 0;
}
