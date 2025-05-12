#include "service_dbus_handle.h"

using DBus::Connection;
using DBus::MethodProxy;
using DBus::ObjectProxy;
using DBus::Path;
using DBus::StandaloneDispatcher;

/**
 * TODO:
 * Find a way to make this happen with DBus Session instead of System i.e. by
 * not using systemd1.Manager.
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
    : svc_nm_(std::move(svc_nm)) {
  dispatch_ = DBus::StandaloneDispatcher::create();
  conn_ = dispatch_->create_connection(DBus::BusType::SYSTEM);
  obj_ = SystemdManager::create(conn_);
}

bool ServiceDBusHandler::is_service() {
  bool ret = false;
  try {
    obj_->get_unit(svc_nm_);
    ret = true;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }
  return ret;
}

bool ServiceDBusHandler::restart_service() {
  bool ret = false;
  try {
    std::string mode = SystemdManager::MODE_REPLACE;
    obj_->restart_unit(svc_nm_, mode);
    ret = true;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }
  return ret;
}

std::shared_ptr<ServiceDBusHandler> ServiceDBusHandler::create(
    std::string svc_nm) {
  return std::make_shared<ServiceDBusHandler>(svc_nm);
}
