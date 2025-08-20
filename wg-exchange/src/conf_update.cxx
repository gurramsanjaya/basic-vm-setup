#include "conf_update.h"

ConfUpdater &ConfUpdater::update_conf(const Peer &peer)
{

    auto conf_fstrm = boost::filesystem::ofstream(conf_pth_, std::ios_base::app);
    conf_fstrm << '\n';
    conf_fstrm << "[Peer]" << '\n';
    conf_fstrm << "AllowedIps = ";
    for (int i = 0; i < peer.addr_list_size(); i++)
    {
        conf_fstrm << peer.addr_list(i).addr() << '/';
        conf_fstrm << peer.addr_list(i).mask() << ", ";
    }
    if (peer.endpoint().size() > 0)
    {
        conf_fstrm << "Endpoint = " << peer.endpoint();
    }
    conf_fstrm << "PublicKey = " << peer.creds().pub_key() << '\n';
    if (peer.creds().pre_shared_key().size() == 32)
    {
        conf_fstrm << "PresharedKey = " << peer.creds().pre_shared_key() << '\n';
    }
    conf_fstrm << "PersistentKeepalive = 25" << '\n';
    conf_fstrm.flush();
    return *this;
}

// Server variant
ConfUpdater &ConfUpdater::update_conf(const po::variables_map &vm, const std::string &priv_key)
{
    auto conf_fstrm = boost::filesystem::ofstream(conf_pth_, std::ios_base::app);
    conf_fstrm << "[Interface]" << '\n';
    for (auto p : intrfc_keys_.list)
    {
        conf_fstrm << generate_interface_line(p, vm);
    }
    conf_fstrm << "PrivateKey = " << priv_key << '\n' << '\n';
    return *this;
}

// Client variant
ConfUpdater &ConfUpdater::update_conf(const po::variables_map &vm,
                                      const google::protobuf::RepeatedPtrField<Address> &addr_list,
                                      const std::string &priv_key, const std::string &dns)
{
    auto conf_fstrm = boost::filesystem::ofstream(conf_pth_, std::ios_base::app);
    conf_fstrm << "[Interface]" << '\n';
    for (auto p : intrfc_keys_.list)
    {
        if (p == "Address")
        {
            for (auto p : addr_list)
            {
                conf_fstrm << "Address = " << p.addr() << p.mask() << '\n';
            }
        }
        else if (p == "DNS" && dns.length() > 0)
        {
            // Overwrite using incoming from the server
            conf_fstrm << "DNS = " << dns << '\n';
        }
        else
        {
            conf_fstrm << generate_interface_line(p, vm);
        }
    }
    conf_fstrm << "PrivateKey = " << priv_key << '\n' << '\n';
    return *this;
}

std::string ConfUpdater::generate_interface_line(const std::string &param, const po::variables_map &vm)
{
    const std::string key = "Interface." + param;
    std::stringstream ss;
    if (vm.count(key))
    {
        ss << "";
    }
    else if (vm[key].value().type() == boost::typeindex::type_id<std::vector<std::string>>())
    {
        for (auto p : vm[key].as<std::vector<std::string>>())
        {
            ss << param << " = " << p << '\n';
        }
    }
    else
    {
        ss << param << " = " << vm[key].as<std::string>() << '\n';
    }
    return ss.str();
}
