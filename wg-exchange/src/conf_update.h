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
    std::vector<std::string> list;
    StaticInterfaceKeys()
    {
        list = {"Address", "ListenPort", "FwMark", "PreUp", "PostUp", "PreDown", "PostDown"};
    }
    friend class ConfUpdater;
};

/**
 * Both client and server can use this
 */
class ConfUpdater
{
  private:
    std::string conf_fnm_;
    boost::interprocess::file_lock fl_;
    bool server_;
    const static StaticInterfaceKeys intrfc_keys_;

  public:
    ConfUpdater() = delete;
    ConfUpdater(std::string conf_fnm, bool is_server) : conf_fnm_(conf_fnm), server_(is_server)
    {
        if (!boost::filesystem::is_regular_file(conf_fnm))
        {
            throw std::invalid_argument(conf_fnm + " isn't a regular file");
        }
    }
    ConfUpdater &operator<<(const Peer &);
    // Tuple consisting of variable_map, private key, dns if any
    ConfUpdater &operator<<(const std::tuple<po::variables_map, const std::string, const std::string>);

  private:
    std::string generate_interface_line(const std::string &key, const po::variables_map &map);
};
