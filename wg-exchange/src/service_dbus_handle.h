#pragma once

#include <dbus-cxx.h>

#include <boost/thread.hpp>

class ServiceDBusHandler {
 private:
  class SystemdManager;
  std::string service_nm_;
  std::shared_ptr<DBus::Dispatcher> dispatch_ = nullptr;
  std::shared_ptr<DBus::Connection> conn_ = nullptr;
  std::shared_ptr<SystemdManager> obj_ = nullptr;

  inline static boost::mutex mutex_;
  inline static std::shared_ptr<ServiceDBusHandler> instance_ = nullptr;

 protected:
 public:
  ServiceDBusHandler() = delete;
  // TODO: Move this to protected without breaking std::make_shared
  ServiceDBusHandler(std::string service_nm);

  // TODO: Should I make this call every single time the service is restarted?
  bool is_service();
  bool restart_service();
  static std::shared_ptr<ServiceDBusHandler> get_instance(
      std::string service_nm);
};
