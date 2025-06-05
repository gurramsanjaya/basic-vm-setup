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
  bool is_service_ = false;
  std::exception_ptr worker_exp_ = nullptr;

  bool running_ = false;
  boost::thread t_;
  boost::mutex restart_mutex_;
  boost::condition_variable restart_cv_;
  bool needs_restart_ = false;

  inline static boost::mutex inst_mutex_;
  inline static std::shared_ptr<ServiceDBusHandler> instance_ = nullptr;

  void run_worker();
  void stop_worker();
  void restart_service();

 public:
  ServiceDBusHandler() = delete;
  // TODO: Move this to protected without breaking std::make_shared
  ServiceDBusHandler(std::string service_nm);

  // TODO: Should I make this call every single time the service is to be
  // restarted? Mainly as a check to see if external forces have
  // modified/deleted the service.
  bool is_service();
  bool trigger_restart_service();
  static std::shared_ptr<ServiceDBusHandler> get_instance(
      std::string service_nm = "");
  ~ServiceDBusHandler();
};
