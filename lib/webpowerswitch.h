#ifndef __WEBPOWERSWITCH_H__INCLUDED__
#define __WEBPOWERSWITCH_H__INCLUDED__

#include <curl/curl.h>
#include <iomanip>
#include <string>
#include <set>
#include <time.h>
#include <vector>


enum OutletState {
  OUTLET_STATE_OFF = 0,
  OUTLET_STATE_ON = 1,
  OUTLET_STATE_UNKNOWN = -1,
};

class Outlet {
public:
  Outlet() {
  }
  Outlet(int id, const std::string& name, OutletState state)
  : id_(id), name_(name), state_(state) {
  }
  int id() const {
    return id_;
  }
  std::string name() const {
    return name_;
  }
  OutletState state() const {
    return state_;
  }
  bool isOn() const {
    return state() == OUTLET_STATE_ON;
  }

private:
  int id_;
  std::string name_;
  OutletState state_;

  friend std::ostream& operator<<(std::ostream& out, const Outlet& outlet) {
    out << std::setw(2) << outlet.id() << "   ";
    out << (outlet.state() == OUTLET_STATE_ON ? "on " : "off");
    out << "   " << outlet.name();
    return out;
  }
};

class WebPowerSwitch {
public:
  WebPowerSwitch(std::string host);
  WebPowerSwitch(const WebPowerSwitch&) = delete;
  virtual ~WebPowerSwitch();
  void suppressDetectionErrors() {
    suppressDetectionErrors_ = true;
  }
  bool login(std::string username, std::string password);
  void logout();
  bool isLoggedIn() const {
    return loggedIn_;
  }
  void dumpOutlets(std::ostream& ostr);
  std::string host() const {
    return host_;
  }
  std::string username() const {
    return username_;
  }
  std::string password() const {
    return password_;
  }
  std::string name() const {
    if (loggedIn_ == false) {
      return "===not_logged_in===";
    }
    return name_;
  }
  const std::vector<Outlet>& outlets() const {
    return outlets_;
  }
  Outlet* getOutlet(const std::string& name);
  bool on(const std::string& outletName);
  bool off(const std::string& outletName);
  bool toggle(const std::string& outletName);
  void verbose() {
    verbose_ = true;
  }
  void verboseCurl() {
    verboseCurl_ = true;
  }

private:
  std::string prefix_ = {"http://"};
  std::string host_;
  std::string username_;
  std::string password_;
  bool loggedIn_ { false };
  CURLSH* share_ { nullptr };
  std::string name_ = {};
  std::vector<Outlet> outlets_;
  bool suppressDetectionErrors_ = false;
  static const long CURL_TIMEOUT;
  bool verbose_ = false;
  bool verboseCurl_ = false;
  time_t nextBuild_ = 0;

  void buildOutlets(bool force = false);
  bool setState(const Outlet* outlet, OutletState newState);
};


#endif  /*  __WEBPOWERSWITCH_H__INCLUDED__  */
