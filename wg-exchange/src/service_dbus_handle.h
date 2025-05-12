#pragma once
#include <dbus-cxx.h>

#include <iostream>

class ServiceDBusHandler {
 private:
  class SystemdManager;
  std::string svc_nm_;
  std::shared_ptr<DBus::Dispatcher> dispatch_ = nullptr;
  std::shared_ptr<DBus::Connection> conn_ = nullptr;
  std::shared_ptr<SystemdManager> obj_ = nullptr;

 public:
  ServiceDBusHandler(std::string svc_nm);
  bool is_service();
  bool restart_service();
  static std::shared_ptr<ServiceDBusHandler> create(std::string svc_nm);
};
