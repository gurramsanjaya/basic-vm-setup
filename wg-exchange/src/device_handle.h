#pragma once

#include <dbus-cxx.h>
#include <generated/wg_exchange.pb.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>

#include "hash_map.h"
#include "wg_exchange.h"

using boost::uuids::uuid;

class PeerRequest
{
  public:
    Credentials peer_creds;
    ClientConfig peer_config;
};

// basic Hash and Probe functions
class HashFn
{
    size_t operator()(const uuid &key)
    {
        return boost::uuids::hash_value(key);
    }
};

class ProbeFn
{
    size_t operator()(int idx, int retries)
    {
        return idx + retries;
    }
};

class DeviceHandler
{

  private:
    typedef FixedSizeLockFreeHashMap<uuid, PeerRequest, HashFn, ProbeFn> hash_map;
    class SystemdManager;

    std::string device_nm_;
    boost::uuids::random_generator rgen;

    std::string service_nm_;
    std::shared_ptr<DBus::Dispatcher> dispatch_ = nullptr;
    std::shared_ptr<DBus::Connection> conn_ = nullptr;
    std::shared_ptr<SystemdManager> obj_ = nullptr;
    bool is_service_ = false;

    std::string device_fnm_;
    boost::interprocess::file_lock fl;
    boost::lockfree::queue<uuid> req_;
    std::unique_ptr<hash_map> hmap_ = nullptr;
    std::string b64_device_pub_;
    std::string endpoint_;
    std::vector<std::string> dns_;

    std::atomic_bool running_;
    boost::thread t_;
    std::exception_ptr worker_exp_ = nullptr;

    inline static boost::mutex inst_mutex_;
    inline static std::shared_ptr<DeviceHandler> instance_ = nullptr;

    void start_worker();
    void run_worker();
    void stop_worker();
    bool process_request();
    void restart_service();
    DeviceHandler &operator<<(PeerRequest &);

  public:
    DeviceHandler() = delete;
    // TODO: Move this to protected without breaking std::make_shared
    DeviceHandler(std::string device_nm);

    // TODO: Should I make this call every single time the service is to be
    // restarted? Mainly as a check to see if external forces have
    // modified/deleted the service.
    bool is_service();
    std::optional<uuid> add_peer_request(PeerRequest &&);
    static std::weak_ptr<DeviceHandler> get_instance(std::string device_nm = "");
    static void forget_instance();
    ~DeviceHandler();
};
