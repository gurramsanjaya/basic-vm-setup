#pragma once

#include <dbus-cxx.h>
#include <generated/wg_exchange.pb.h>

#include <boost/lockfree/queue.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>

#include "conf_update.h"
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
  public:
    size_t operator()(const uuid &key)
    {
        return boost::uuids::hash_value(key);
    }
};

class ProbeFn
{
  public:
    size_t operator()(int idx, int retries)
    {
        return idx + retries;
    }
};

// Explicit initialization
template class FixedSizeLockFreeHashMap<uuid, PeerRequest, HashFn, ProbeFn>;
typedef FixedSizeLockFreeHashMap<uuid, PeerRequest, HashFn, ProbeFn> fhash_map;

class DeviceHandler
{

  private:
    class SystemdManager;

    std::string device_nm_;
    boost::uuids::random_generator rgen;

    std::string service_nm_;
    std::shared_ptr<DBus::Dispatcher> dispatch_ = nullptr;
    std::shared_ptr<DBus::Connection> conn_ = nullptr;
    std::shared_ptr<SystemdManager> obj_ = nullptr;
    bool is_setup_ = false;

    std::string device_pth_;
    std::shared_ptr<ConfUpdater> conf_updater_ = nullptr;
    boost::lockfree::queue<uuid, boost::lockfree::capacity<100>> req_;
    fhash_map::SomePointer hmap_ = nullptr;
    std::string b64_device_pub_;
    std::string endpoint_;
    std::vector<std::string> dns_;

    std::atomic_bool running_;
    std::thread t_;
    std::exception_ptr worker_exp_ = nullptr;

    inline static std::mutex inst_mutex_;
    inline static std::shared_ptr<DeviceHandler> instance_ = nullptr;

    void start_worker();
    void run_worker();
    void stop_worker();
    bool process_request();
    void restart_service();
    DeviceHandler &operator<<(PeerRequest &);

  public:
    DeviceHandler() = delete;

    DeviceHandler(std::string device_nm)
    {
        device_nm_ = std::move(device_nm);
        device_pth_ = "/etc/wireguard/" + device_nm_ + ".conf";
        service_nm_ = "wg-quick@" + device_nm_ + ".service";
        hmap_ = fhash_map::create(31);
    }

    ~DeviceHandler()
    {
        stop_worker();
    }

    static std::weak_ptr<DeviceHandler> get_instance(std::string device_nm = "")
    {
        std::lock_guard<std::mutex> lock(inst_mutex_);
        if (!instance_)
        {
            instance_ = std::make_shared<DeviceHandler>(device_nm);
        }
        return instance_;
    }

    static void forget_instance()
    {
        std::lock_guard<std::mutex> lock(inst_mutex_);
        // Might not be deleted here, some other weak_ptr holder
        // might have upgraded temporarily to shared_ptr before then.
        instance_ = nullptr;
    }

    bool setup(po::variables_map &);
    std::optional<uuid> add_peer_request(PeerRequest &&);
};
