#pragma once

#include <generated/wg_exchange.pb.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "wg_exchange.h"

namespace po = boost::program_options;

/**
 * Just a convenient and trashy way to make this static
 */
class StaticInterfaceKeys
{
  protected:
    std::vector<std::string> list;
    StaticInterfaceKeys()
    {
        list = {"Address", "ListenPort", "DNS", "FwMark", "PreUp", "PostUp", "PreDown", "PostDown"};
    }
    friend class ConfUpdater;
};

/**
 * Both client and server can use this
 */
class ConfUpdater
{
  private:
    boost::filesystem::path conf_pth_;
    boost::interprocess::file_lock fl_;
    inline const static StaticInterfaceKeys intrfc_keys_;

  public:
    ConfUpdater() = delete;
    ConfUpdater(std::string conf_pth) : conf_pth_(conf_pth)
    {
        if (!boost::filesystem::is_regular_file(conf_pth_))
        {
            const boost::filesystem::path prnt_pth = conf_pth_.parent_path();
            if (prnt_pth == conf_pth_)
            {
                throw std::invalid_argument("invalid conf path");
            }
            boost::filesystem::create_directories(prnt_pth);
            (boost::filesystem::ofstream(conf_pth_));
        }
        else
        {
            (boost::filesystem::ofstream(conf_pth_, std::ios_base::trunc));
        }
    }
    // operator<<() is very limited in the number of arguments it can take
    // using equivalent method that does the same, but with overloading
    ConfUpdater &update_conf(const Peer &peer);

    // Server usage only
    ConfUpdater &update_conf(const po::variables_map &vmap, const std::string &priv_key);

    // Client usage only
    ConfUpdater &update_conf(const po::variables_map &vmap,
                             const google::protobuf::RepeatedPtrField<Address> &addr_list, const std::string &priv_key,
                             const std::string &dns = "");

  private:
    std::string generate_interface_line(const std::string &key, const po::variables_map &vmap);
};