#include <iostream>
#include <vector>

#define VAL_MSG_ITR_GET(val, itr, get_func, ...) \
  do {                                           \
    val = itr.get_func(__VA_ARGS__);             \
    itr.next();                                  \
  } while (0)

#define MSG_ITR_GET(itr, get_func, ...) \
  do {                                  \
    itr.get_func(__VA_ARGS__);          \
    itr.next();                         \
  } while (0)

typedef struct Changes {
  std::string type;
  std::string file_nm;
  std::string dest;
} Changes;

typedef struct Enabled {
  bool carries_install_info;
  std::vector<Changes> changes;
} Enabled;

// Needs to be placed before the dbus-cxx.h include
namespace Dbus {
inline std::string signature(Enabled) { return "ba(sss)"; }
}  // namespace Dbus

#include <boost/thread.hpp>

#include "service_dbus_handle.h"

using DBus::Connection;
using DBus::MessageIterator;
using DBus::MethodProxy;
using DBus::MultipleReturn;
using DBus::ObjectProxy;
using DBus::Path;
using DBus::StandaloneDispatcher;

// (sss) is STRUCT type, so needs to recurse
MessageIterator& operator>>(MessageIterator& itr, Changes& val) {
  MessageIterator sub_itr = itr.recurse();
  VAL_MSG_ITR_GET(val.type, sub_itr, get_string);
  VAL_MSG_ITR_GET(val.file_nm, sub_itr, get_string);
  VAL_MSG_ITR_GET(val.dest, sub_itr, get_string, );
  return itr;
}

MessageIterator& operator>>(MessageIterator& itr, Enabled& val) {
  VAL_MSG_ITR_GET(val.carries_install_info, itr, get_bool);
  MSG_ITR_GET(itr, get_array, val.changes);
  return itr;
}

/**
 * TODO:
 * Find a way to make this happen with DBus Session instead of System i.e. by
 * not using org.freedesktop.systemd1.Manager.
 */
class ServiceDBusHandler::SystemdManager : public ObjectProxy {
 protected:
  std::shared_ptr<MethodProxy<Path(std::string)>> get_unit_ = nullptr;
  std::shared_ptr<MethodProxy<Path(std::string, std::string)>> start_unit_ =
      nullptr;
  std::shared_ptr<MethodProxy<Path(std::string, std::string)>> stop_unit_ =
      nullptr;
  std::shared_ptr<MethodProxy<Path(std::string, std::string)>> restart_unit_ =
      nullptr;
  std::shared_ptr<MethodProxy<Enabled(std::vector<std::string>, bool, bool)>>
      enable_units_ = nullptr;
  std::shared_ptr<MethodProxy<void(void)>> reload_ = nullptr;

  SystemdManager(std::shared_ptr<Connection> conn)
      : ObjectProxy(conn, "org.freedesktop.systemd1",
                    "/org/freedesktop/systemd1") {
    get_unit_ = this->create_method<Path(std::string)>(
        "org.freedesktop.systemd1.Manager", "GetUnit");
    start_unit_ = this->create_method<Path(std::string, std::string)>(
        "org.freedesktop.systemd1.Manager", "StartUnit");
    stop_unit_ = this->create_method<Path(std::string, std::string)>(
        "org.freedesktop.systemd1.Manager", "StopUnit");
    restart_unit_ = this->create_method<Path(std::string, std::string)>(
        "org.freedesktop.systemd1.Manager", "RestartUnit");
    enable_units_ =
        this->create_method<Enabled(std::vector<std::string>, bool, bool)>(
            "org.freedesktop.systemd1.Manager", "EnableUnitFiles");
    reload_ = this->create_method<void(void)>(
        "org.freedesktop.systemd1.Manager", "Reload");
  }

 public:
  inline static const std::string MODE_REPLACE{"replace"};
  inline static const std::string MODE_FAIL{"fail"};

  static std::shared_ptr<SystemdManager> create(
      std::shared_ptr<Connection> conn) {
    return std::shared_ptr<SystemdManager>(new SystemdManager(conn));
  }
  Path get_unit(std::string& name) { return (*get_unit_)(name); }
  Path start_unit(std::string& name, std::string& mode) {
    return (*start_unit_)(name, mode);
  }
  Path stop_unit(std::string& name, std::string& mode) {
    return (*stop_unit_)(name, mode);
  }
  Path restart_unit(std::string& name, std::string& mode) {
    return (*restart_unit_)(name, mode);
  }

  Enabled enable_units(std::vector<std::string> units, bool runtime,
                       bool force) {
    return (*enable_units_)(units, runtime, force);
  }

  void daemon_reload() { (*reload_)(); }
};

// class ServiceHandler::ServiceUnit : public ObjectProxy {
//  protected:
//   ServiceUnit(std::shared_ptr<Connection> conn, Path& svc_path)
//       : ObjectProxy(conn, "org.freedesktop.systemd1", svc_path) {}

//  public:
//   static std::shared_ptr<ServiceUnit> create(std::shared_ptr<Connection>
//   conn,
//                                              Path& svc_path) {
//     return std::shared_ptr<ServiceUnit>(new ServiceUnit(conn, svc_path));
//   }
// };

ServiceDBusHandler::ServiceDBusHandler(std::string svc_nm)
    : service_nm_(std::move(svc_nm)) {
  dispatch_ = DBus::StandaloneDispatcher::create();
  conn_ = dispatch_->create_connection(DBus::BusType::SYSTEM);
  obj_ = SystemdManager::create(conn_);
  running_ = true;
}

void ServiceDBusHandler::run_worker() {
  boost::chrono::seconds sleep_time(5);
  bool nr = false;
  while (running_) {
    {
      boost::unique_lock lock(restart_mutex_);
      restart_cv_.wait_for(lock, sleep_time,
                           [this] { return this->needs_restart_; });
      nr = needs_restart_;
      needs_restart_ = false;
    }
    if (nr) {
      restart_service();
    }
  }
  {
    boost::lock_guard lock(restart_mutex_);
    nr = needs_restart_;
    needs_restart_ = false;  // Not really required
  }
  if (nr) {
    // Are we sure it will even come here? Because running_ = false is only set
    // in the destructor. And 'usually' methods shouldn't be accessed after
    // desctructor is called. I'm a little worried about making the desctructor
    // wait for even a bit.
    boost::this_thread::sleep_for(boost::chrono::seconds(1));
    restart_service();
  }
}

void ServiceDBusHandler::stop_worker() {
  running_ = false;
  if (t_.joinable()) {
    std::cerr << "shutting down the ServiceDBusHandler" << '\n';
    t_.join();
  }
}

void ServiceDBusHandler::restart_service() {
  try {
    std::string mode = SystemdManager::MODE_REPLACE;
    obj_->restart_unit(service_nm_, mode);
  } catch (const std::exception& e) {
    worker_exp_ = std::current_exception();
  }
}

bool ServiceDBusHandler::is_service() {
  if (!is_service_) {
    try {
      obj_->get_unit(service_nm_);
      is_service_ = true;
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';

      // If it throws an exception here, let it go unhandled.
      std::cerr << "trying to enable the service " << service_nm_
                << " permanently" << '\n';
      std::vector<std::string> units(1, service_nm_);
      Enabled enabled = obj_->enable_units(units, false, false);

      std::cerr << enabled.changes[0].type
                << ", name: " << enabled.changes[0].file_nm
                << ", destination: " << enabled.changes[0].dest << '\n';
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

      obj_->daemon_reload();
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

      obj_->get_unit(service_nm_);
      is_service_ = true;
    }

    // Starting the separate thread here instead of the constructor
    t_ = boost::thread(&ServiceDBusHandler::run_worker, this);
  }
  return is_service_;
}

bool ServiceDBusHandler::trigger_restart_service() {
  // Transporting worker exception to main/gRPC thread
  if (worker_exp_) {
    std::rethrow_exception(worker_exp_);
  }
  std::lock_guard lock(restart_mutex_);
  needs_restart_ = true;
  restart_cv_.notify_one();
}

std::shared_ptr<ServiceDBusHandler> ServiceDBusHandler::get_instance(
    std::string service_nm) {
  boost::lock_guard<boost::mutex> lock(inst_mutex_);
  if (!instance_) {
    instance_ = std::make_shared<ServiceDBusHandler>(service_nm);
  }
  return instance_;
}

ServiceDBusHandler::~ServiceDBusHandler() { stop_worker(); }
