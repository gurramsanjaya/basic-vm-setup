#include <iostream>
#include <unistd.h>
#include <vector>

#include "ext_hacl_star/Hacl_Curve25519_51.h"

#define VAL_MSG_ITR_GET(val, itr, get_func, ...)                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        val = itr.get_func(__VA_ARGS__);                                                                               \
        itr.next();                                                                                                    \
    } while (0)

#define MSG_ITR_GET(itr, get_func, ...)                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        itr.get_func(__VA_ARGS__);                                                                                     \
        itr.next();                                                                                                    \
    } while (0)

typedef struct Changes
{
    std::string type;
    std::string file_nm;
    std::string dest;
} Changes;

typedef struct Enabled
{
    bool carries_install_info;
    std::vector<Changes> changes;
} Enabled;

// Needs to be placed before the dbus-cxx.h include
namespace Dbus
{
inline std::string signature(Enabled)
{
    return "ba(sss)";
}
} // namespace Dbus

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <thread>

#include "device_handle.h"

using DBus::Connection;
using DBus::MessageIterator;
using DBus::MethodProxy;
using DBus::MultipleReturn;
using DBus::ObjectProxy;
using DBus::Path;
using DBus::StandaloneDispatcher;

// (sss) is STRUCT type, so needs to recurse
MessageIterator &operator>>(MessageIterator &itr, Changes &val)
{
    MessageIterator sub_itr = itr.recurse();
    VAL_MSG_ITR_GET(val.type, sub_itr, get_string);
    VAL_MSG_ITR_GET(val.file_nm, sub_itr, get_string);
    VAL_MSG_ITR_GET(val.dest, sub_itr, get_string, );
    return itr;
}

MessageIterator &operator>>(MessageIterator &itr, Enabled &val)
{
    VAL_MSG_ITR_GET(val.carries_install_info, itr, get_bool);
    MSG_ITR_GET(itr, get_array, val.changes);
    return itr;
}

/**
 * Class DeviceHadnler::SystemdManager
 * TODO:
 * Find a way to make this happen with DBus Session instead of System i.e. by
 * not using org.freedesktop.systemd1.Manager.
 */
class DeviceHandler::SystemdManager : public ObjectProxy
{
  protected:
    std::shared_ptr<MethodProxy<Path(std::string)>> get_unit_ = nullptr;
    std::shared_ptr<MethodProxy<Path(std::string, std::string)>> start_unit_ = nullptr;
    std::shared_ptr<MethodProxy<Path(std::string, std::string)>> stop_unit_ = nullptr;
    std::shared_ptr<MethodProxy<Path(std::string, std::string)>> restart_unit_ = nullptr;
    std::shared_ptr<MethodProxy<Enabled(std::vector<std::string>, bool, bool)>> enable_units_ = nullptr;
    std::shared_ptr<MethodProxy<void(void)>> reload_ = nullptr;

    SystemdManager(std::shared_ptr<Connection> conn)
        : ObjectProxy(conn, "org.freedesktop.systemd1", "/org/freedesktop/systemd1")
    {
        get_unit_ = this->create_method<Path(std::string)>("org.freedesktop.systemd1.Manager", "GetUnit");
        start_unit_ =
            this->create_method<Path(std::string, std::string)>("org.freedesktop.systemd1.Manager", "StartUnit");
        stop_unit_ =
            this->create_method<Path(std::string, std::string)>("org.freedesktop.systemd1.Manager", "StopUnit");
        restart_unit_ =
            this->create_method<Path(std::string, std::string)>("org.freedesktop.systemd1.Manager", "RestartUnit");
        enable_units_ = this->create_method<Enabled(std::vector<std::string>, bool, bool)>(
            "org.freedesktop.systemd1.Manager", "EnableUnitFiles");
        reload_ = this->create_method<void(void)>("org.freedesktop.systemd1.Manager", "Reload");
    }

  public:
    inline static const std::string MODE_REPLACE{"replace"};
    inline static const std::string MODE_FAIL{"fail"};

    static std::shared_ptr<SystemdManager> create(std::shared_ptr<Connection> conn)
    {
        return std::shared_ptr<SystemdManager>(new SystemdManager(conn));
    }
    Path get_unit(std::string &name)
    {
        return (*get_unit_)(name);
    }
    Path start_unit(std::string &name, std::string &mode)
    {
        return (*start_unit_)(name, mode);
    }
    Path stop_unit(std::string &name, std::string &mode)
    {
        return (*stop_unit_)(name, mode);
    }
    Path restart_unit(std::string &name, std::string &mode)
    {
        return (*restart_unit_)(name, mode);
    }

    Enabled enable_units(std::vector<std::string> units, bool runtime, bool force)
    {
        return (*enable_units_)(units, runtime, force);
    }

    void daemon_reload()
    {
        (*reload_)();
    }
};

/**
 * Class DeviceHandler
 */
// I'll rework this soon, such that the restart_service is run only once maybe
void DeviceHandler::run_worker()
{
    std::chrono::seconds process_time(30);
    std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    int counter = 0;
    while (running_.load(std::memory_order_relaxed))
    {
        if (start + process_time < std::chrono::steady_clock::now() && counter > 0)
        {
            restart_service();
            counter = 0;
        }
        else if (process_request())
        {
            counter++;
        }
    }
    // stop even if there are any requests pending, but don't forget to restart if
    // some have already been processed
    if (counter > 0)
    {
        restart_service();
    }
}

void DeviceHandler::start_worker()
{
    std::cerr << "starting DeviceHandler..." << '\n';
    running_.store(true, std::memory_order_relaxed);
    t_ = std::thread(&DeviceHandler::run_worker, this);
}

void DeviceHandler::stop_worker()
{
    running_.store(false, std::memory_order_relaxed);
    if (t_.joinable())
    {
        std::cerr << "shutting down the DeviceHandler..." << '\n';
        t_.join();
    }
    std::cerr << "shutdown of DeviceHandler complete..." << '\n';
}

bool DeviceHandler::process_request()
{
    uuid key;
    bool success = false;
    if (req_.pop(key))
    {
        // May fail even with strong
        auto it = hmap_->find(key, false);
        auto pair = *it;
        if (pair.has_value())
        {
            PeerRequest value{std::get<1>(pair.value())};
            // Actual process here
            Peer *peer = new Peer();
            peer->set_endpoint(endpoint_);
            Credentials *creds = new Credentials();
            creds->set_pub_key(b64_device_pub_);
            creds->set_pre_shared_key(value.peer_creds.pre_shared_key());
            peer->set_allocated_creds(creds);
            value.peer_config.set_allocated_peer(peer);
            for (auto each : dns_)
            {
                value.peer_config.add_dns(each);
            }

            conf_updater_->update_conf(*peer);

            std::pair<fhash_map::Iterator, bool> res;
            do
            {
                res = hmap_->update(std::pair(key, value));
                // keep looping till contention issue gets resolved
            } while (res.second == false && res.first != hmap_->end());
            success = true;
        }
    }
    return success;
}

void DeviceHandler::restart_service()
{
    try
    {
        std::cerr << "restarting service..." << '\n';
        std::string mode = SystemdManager::MODE_REPLACE;
        obj_->restart_unit(service_nm_, mode);
        std::cerr << "restarted service..." << '\n';
    }
    catch (const std::exception &e)
    {
        worker_exp_ = std::current_exception();
    }
}

bool DeviceHandler::setup(po::variables_map &vmap)
{
    if (!is_setup_)
    {
        conf_updater_ = std::make_shared<ConfUpdater>(device_pth_);

        uint8_t key[WG_KEY_LEN], pub_key[WG_KEY_LEN];
        char base64_pub[WG_KEY_LEN_BASE64], base64_key[WG_KEY_LEN_BASE64];

        if (!generate_random_key(key))
        {
            throw std::runtime_error("key generation failed");
        }
        Hacl_Curve25519_51_secret_to_public(pub_key, key);

        key_to_base64(base64_key, key);
        key_to_base64(base64_pub, pub_key);

        std::string b64_device_key{base64_key, WG_KEY_LEN_BASE64};
        conf_updater_->update_conf(vmap, b64_device_key);
        b64_device_pub_ = std::string(base64_pub, WG_KEY_LEN_BASE64);

        dispatch_ = DBus::StandaloneDispatcher::create();
        conn_ = dispatch_->create_connection(DBus::BusType::SYSTEM);
        obj_ = SystemdManager::create(conn_);

        try
        {
            obj_->get_unit(service_nm_);
            is_setup_ = true;
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';

            // If it throws an exception here, let it go unhandled.
            std::cerr << "trying to enable the service " << service_nm_ << " permanently" << '\n';
            std::vector<std::string> units(1, service_nm_);
            Enabled enabled = obj_->enable_units(units, false, false);

            std::cerr << enabled.changes[0].type << ", name: " << enabled.changes[0].file_nm
                      << ", destination: " << enabled.changes[0].dest << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            obj_->daemon_reload();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            obj_->get_unit(service_nm_);
            is_setup_ = true;
        }

        // Starting the separate thread here instead of the constructor
        start_worker();
    }
    return is_setup_;
}

std::optional<uuid> DeviceHandler::add_peer_request(PeerRequest &&peer_req)
{
    // Transporting worker exception to main/gRPC thread
    if (worker_exp_)
    {
        std::rethrow_exception(worker_exp_);
    }
    uuid key(rgen());
    auto res = hmap_->insert(std::make_pair(key, std::move(peer_req)));
    if (std::get<1>(res))
    {
        return std::optional<uuid>(std::move(key));
    }
    return std::optional<uuid>();
}
