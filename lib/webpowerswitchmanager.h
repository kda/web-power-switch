#ifndef __WEBPOWERSWITCHMANAGER_H__INCLUDED__
#define __WEBPOWERSWITCHMANAGER_H__INCLUDED__

#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include "webpowerswitch.h"


class WebPowerSwitchManager {
public:
  WebPowerSwitchManager()
  : WebPowerSwitchManager(true, true) {}
  WebPowerSwitchManager(bool enableCache, bool findSwitches);
  ~WebPowerSwitchManager();
  bool addUsernamePassword(std::string username, std::string password);
  bool load();
  void resetCache();
  WebPowerSwitch* getSwitch(std::string name, bool allow_miss = false);
  WebPowerSwitch* getSwitchByIp(std::string_view ip, bool allow_miss = false);
  WebPowerSwitch* getSwitchByOutletName(std::string name);
  Outlet* getOutletByName(std::string name);
  void dumpSwitches(std::ostream& ostr);
  void verbose(int increment = 1) {
    verbose_ += increment;
  }

private:
  bool enableCache_ = true;
  bool resetCache_ = false;
  bool findSwitches_ = true;
  std::unordered_map<std::string, std::unique_ptr<WebPowerSwitch>> mNameToSwitch_;
  struct UsernamePassword {
    std::string username;
    std::string password;
  };
  std::vector<UsernamePassword> vUsernamePassword_;
  std::string cacheFile_ = {};
  YAML::Node cache_;
  static const char* CACHE_KEY_CONTROLLERS;
  static const char* CACHE_KEY_CONTROLLERBYNAME;
  static const char* CACHE_CONTROLLERBYNAME_KEY_HOST;
  static const char* CACHE_KEY_OUTLETS;
  static const char* CACHE_OUTLETS_KEY_CONTROLLER;
  static const char* CACHE_OUTLETS_KEY_ID;
  const time_t cacheTimeout_ = (60 * 60) * 24;
  int verbose_ { 0 };
  int fdWrite_ = -1;

  bool isCacheLoaded();
  bool validateCacheFile();
  void loadCache();
  void writeCacheStart();
  void writeCacheFinish();
  void findSwitches();
  std::string getDefaultInterface();
  void getIpAddressAndSubnetMask(std::string interface, std::string& ipAddress, std::string& subNetMask);
  WebPowerSwitch* connectSwitch(std::string_view ip);
};

#endif  /*  __WEBPOWERSWITCHMANAGER_H__INCLUDED__  */
