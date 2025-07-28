#include "conf_update.h"

ConfUpdater &ConfUpdater::operator<<(const Peer &peer)
{

    boost::lock_guard<boost::interprocess::file_lock> lock(fl_);
    auto conf_fstrm = boost::filesystem::ofstream(conf_fnm_);
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

ConfUpdater &ConfUpdater::operator<<(const std::tuple<po::variables_map, const std::string, const std::string> tuple)
{
    boost::lock_guard<boost::interprocess::file_lock> lock(fl_);
    auto conf_fstrm = boost::filesystem::ofstream(conf_fnm_);
    auto vm = std::get<0>(tuple);
    conf_fstrm << "[Interface]" << '\n';
    for (auto p : intrfc_keys_.list)
    {
        conf_fstrm << generate_interface_line(p, vm);
    }
    // Server doesn't need the dns
    if (!server_)
    {
        std::string dns_line = generate_interface_line("DNS", vm);
        if (dns_line.length() > 0)
        {
            conf_fstrm << dns_line;
        }
        else
        {
            conf_fstrm << "DNS = " << std::get<2>(tuple) << '\n';
        }
    }
    conf_fstrm << "PrivateKey = " << std::get<1>(tuple) << '\n' << '\n';
    return *this;
}

std::string ConfUpdater::generate_interface_line(const std::string &param, const po::variables_map &vm)
{
    const std::string key = "Interface." + param;
    std::stringstream ss;
    if (vm.count(key))
    {
        return "";
    }
    if (vm[key].value().type() == boost::typeindex::type_id<std::vector<std::string>>())
    {
        for (auto p : vm[key].as<std::vector<std::string>>())
        {
            ss << param << " = " << p << '\n';
        }
    }
    else
    {
        ss << param << " = " << vm[key].as<std::string>() + '\n';
    }
    return ss.str();
}
