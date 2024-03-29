#ifndef __WEBPOWERSWITCH_H__INCLUDED__
#define __WEBPOWERSWITCH_H__INCLUDED__

#include <absl/strings/string_view.h>
#include <curl/curl.h>
#include <iomanip>
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
  Outlet(int id, absl::string_view name, OutletState state)
  : id_(id), name_(name), state_(state) {
  }
  int id() const {
    return id_;
  }
  absl::string_view name() const {
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
  OutletState state_ = OUTLET_STATE_UNKNOWN;

  friend std::ostream& operator<<(std::ostream& out, const Outlet& outlet) {
    out << std::setw(2) << outlet.id() << "   ";
    out << (outlet.state() == OUTLET_STATE_ON ? "on " : "off");
    out << "   " << outlet.name();
    return out;
  }
};

class WebPowerSwitch {
public:
  WebPowerSwitch(absl::string_view host);
  WebPowerSwitch(const WebPowerSwitch&) = delete;
  virtual ~WebPowerSwitch();
  void suppressDetectionErrors() {
    suppressDetectionErrors_ = true;
  }
  bool login(absl::string_view username, absl::string_view password);
  CURL* startLogin(absl::string_view username, absl::string_view password);
  CURL* next();
  void logout();
  bool isLoggedIn() const {
    return loggedIn_;
  }
  void dumpOutlets(std::ostream& ostr);
  absl::string_view host() const {
    return host_;
  }
  CURL* handle() {
    return request_;
  }
  absl::string_view name() const {
    if (loggedIn_ == false) {
      return "===not_logged_in===";
    }
    return name_;
  }
  const std::vector<Outlet>& outlets() const {
    return outlets_;
  }
  Outlet* getOutlet(absl::string_view name);
  bool on(absl::string_view outletName);
  bool off(absl::string_view outletName);
  bool toggle(absl::string_view outletName);
  void verbose(int increment = 1) {
    verbose_ += increment;
  }

private:
  enum State {
    STATE_UNINITIALIZED = 0,
    STATE_INITIAL_PAGE_REQUESTED,
    STATE_LOGIN_REQUESTED,
    STATE_LOGGED_IN,
    STATE_LOGIN_FAILED,
    STATE_OUTLETS_BUILT,
  };
  State state_ = STATE_UNINITIALIZED;
  std::string prefix_ = {"http://"};
  std::string host_;
  CURL* request_ = nullptr;
  std::ostringstream os_;
  std::string username_;
  std::string password_;
  bool loggedIn_ = false;
  CURLSH* share_ = nullptr;
  std::string name_ = {};
  std::vector<Outlet> outlets_;
  bool suppressDetectionErrors_ = false;
  static const long CURL_TIMEOUT;
  int verbose_ { 0 };
  time_t nextBuild_ = 0;

  void initializeRequest();
  void clearRequest();
	void dumpCookies();
  void prepToFetchOutlets();
  void buildOutlets();
  bool setState(const Outlet* outlet, OutletState newState);
  bool detectionErrorsAreSuppressed() {
    return suppressDetectionErrors_ && verbose_ == 0;
  }
};


#endif  /*  __WEBPOWERSWITCH_H__INCLUDED__  */
