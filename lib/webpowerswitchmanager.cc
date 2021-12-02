#include "webpowerswitchmanager.h"

#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <iostream>
#include <netdb.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>


const char* WebPowerSwitchManager::CACHE_KEY_CONTROLLERS = "controllers";
const char* WebPowerSwitchManager::CACHE_KEY_CONTROLLERBYNAME = "controller_by_name";
const char* WebPowerSwitchManager::CACHE_CONTROLLERBYNAME_KEY_HOST = "host";
const char* WebPowerSwitchManager::CACHE_KEY_OUTLETS = "outlets";
const char* WebPowerSwitchManager::CACHE_OUTLETS_KEY_CONTROLLER = "controller";
const char* WebPowerSwitchManager::CACHE_OUTLETS_KEY_ID = "id";

WebPowerSwitchManager::WebPowerSwitchManager(bool enableCache, bool findSwitches)
: enableCache_(enableCache), findSwitches_(findSwitches) {
}

WebPowerSwitchManager::~WebPowerSwitchManager() {
}

bool WebPowerSwitchManager::addUsernamePassword(std::string username, std::string password) {
  vUsernamePassword_.push_back({username, password});
  return true;
}

bool WebPowerSwitchManager::load() {
  if (isCacheLoaded()) {
    return true;
  }

  loadCache();
  if (isCacheLoaded() == false) {
    writeCacheStart();
    findSwitches();
    // walk added switches to extend cache
    writeCacheFinish();
  }

  return true;
}

void WebPowerSwitchManager::resetCache() {
  if (isCacheLoaded()) {
    cache_.reset();
  }
  resetCache_ = true;
}

WebPowerSwitch* WebPowerSwitchManager::getSwitch(std::string name, bool allow_miss) {
  if (load() == false) {
    return nullptr;
  }
  auto iter = mNameToSwitch_.find(name);
  if (iter == mNameToSwitch_.end()) {
    if (!cache_[CACHE_KEY_CONTROLLERBYNAME][name]) {
      if (allow_miss == false) {
        std::cerr << "ERROR: unknown switch name: " << name << std::endl;
      }
      return nullptr;
    }
    auto wps = std::make_unique<WebPowerSwitch>(cache_[CACHE_KEY_CONTROLLERBYNAME][name][CACHE_CONTROLLERBYNAME_KEY_HOST].as<std::string>());
    wps->verbose(verbose_);
    for (auto up : vUsernamePassword_) {
      // TODO(kda): move to method on webpowerswitch
      // first page
      auto request = wps->login(up.username, up.password);
      if (request == nullptr) {
        continue;
      }
      if (curl_easy_perform(request) != CURLE_OK) {
        continue;
      }
      // attempt login
      request = wps->next();
      if (curl_easy_perform(request) != CURLE_OK) {
        continue;
      }
      // login succeeded, fetch outlets
      request = wps->next();
      if (curl_easy_perform(request) != CURLE_OK) {
        continue;
      }
      // all done
      if (wps->next() == nullptr) {
        break;
      } else {
        std::cerr << "unexpected: " << wps->name() << std::endl;
      }
    }
    if (wps->isLoggedIn() == false) {
      std::cerr << "ERROR: login failed switch name: " << name << std::endl;
      return nullptr;
    }
    mNameToSwitch_[name].reset(wps.release());
    iter = mNameToSwitch_.find(name);
  }
  return iter->second.get();
}

WebPowerSwitch* WebPowerSwitchManager::getSwitchByOutletName(std::string name) {
  if (load() == false) {
    return nullptr;
  }
  if (!cache_[CACHE_KEY_OUTLETS] || !cache_[CACHE_KEY_OUTLETS][name]) {
    //std::cout << "unknown outlet name: " << name << std::endl;
    return nullptr;
  }
  return getSwitch(cache_[CACHE_KEY_OUTLETS][name][CACHE_OUTLETS_KEY_CONTROLLER].as<std::string>());
}

Outlet* WebPowerSwitchManager::getOutletByName(std::string name) {
  auto wps = getSwitchByOutletName(name);
  if (wps == nullptr) {
    return nullptr;
  }
  return wps->getOutlet(name);
}

void WebPowerSwitchManager::dumpSwitches(std::ostream& ostr) {
  if (load() == false) {
    std::cerr << "no switches found or able to load" << std::endl;
    return;
  }
  auto cbnCache = cache_[CACHE_KEY_CONTROLLERBYNAME];
  //std::cout << "cbnCache: " << cbnCache << std::endl;
  for (auto controller : cbnCache) {
    //std::cout << "controller: " << controller.first.as<std::string>() << std::endl;
    //std::cout << "host: " << cbnCache[controller.first][CACHE_CONTROLLERBYNAME_KEY_HOST].as<std::string>() << std::endl;
    auto wps = getSwitch(controller.first.as<std::string>());
    if (wps != nullptr) {
      wps->dumpOutlets(ostr);
    }
  }
}

bool WebPowerSwitchManager::isCacheLoaded() {
  return (cache_.size() > 0);
}

bool WebPowerSwitchManager::validateCacheFile() {
  const char* tmpdir = getenv("TMPDIR");
  if (tmpdir == nullptr) {
    tmpdir = "/tmp";
  }
  //std::cout << "tmpdir: " << tmpdir << std::endl;
  //std::string cacheDirectory = "/var/cache/webpowerswitchcontrol/";
  std::string cacheDirectory = std::string(tmpdir) + "/webpowerswitchcontrol/";
  DIR* dir = opendir(cacheDirectory.c_str());
  if (dir == nullptr) {
    if (errno == ENOENT) {
      // TODO: check return value
      mkdir(cacheDirectory.c_str(), 0777);
      chmod(cacheDirectory.c_str(), 0777);
    } else {
      std::cout << "unable to create cache directory: " << cacheDirectory << std::endl;
      return false;
    }
  } else {
    closedir(dir);
  }
  cacheFile_ = cacheDirectory + "cache.yaml";
  return true;
}

void WebPowerSwitchManager::loadCache() {
  if (enableCache_ == false) {
    return;
  }
  if (resetCache_) {
    resetCache_ = false;
    return;
  }
  if (validateCacheFile() == false) {
    return;
  }
  struct stat statCacheFile;
  auto statRetval = stat(cacheFile_.c_str(), &statCacheFile);
  if (statRetval != 0) {
    return;
  }
  auto tooOld = time(nullptr) - cacheTimeout_;
  if (statCacheFile.st_mtime <= tooOld) {
    return;
  }

  auto fd = open(cacheFile_.c_str(), O_RDONLY);
  if (fd < 0) {
    std::cerr << "failed to open cache: " << cacheFile_ << std::endl;
    return;
  }
  auto r = flock(fd, LOCK_SH | LOCK_NB);
  if (r < 0) {
    std::cerr << "failed to obtain write lock: " << cacheFile_ << std::endl;
    close(fd);
    return;
  }
  std::stringstream ss;
  const int BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE];
  ssize_t readsize;
  while ((readsize = read(fd, buffer, BUFFER_SIZE)) > 0) {
    ss << std::string(buffer, readsize);
  }
  close(fd);
  //std::cout << "ss.str(): " << ss.str() << std::endl;

  try {
    cache_ = YAML::Load(ss.str());
  } catch (...) {
    std::cerr << "failed to load cache: " << cacheFile_ << std::endl;
  }
}

void WebPowerSwitchManager::writeCacheStart() {
  if (enableCache_ == false) {
    return;
  }
  if (validateCacheFile() == false) {
    return;
  }
  fdWrite_ = open(cacheFile_.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
  auto r = flock(fdWrite_, LOCK_EX | LOCK_NB);
  if (r < 0) {
    std::cerr << "failed to obtain write lock: " << cacheFile_ << std::endl;
    close(fdWrite_);
    return;
  }
}

void WebPowerSwitchManager::writeCacheFinish() {
  if (fdWrite_ < 0) {
    return;
  }
  if (isCacheLoaded() == false) {
    close(fdWrite_);
    fdWrite_ = -1;
    return;
  }

  std::stringstream ss;
  ss << cache_;
  auto output = ss.str();
  if (write(fdWrite_, output.c_str(), output.length()) != static_cast<ssize_t>(output.length())) {
    std::cerr << "failed to write cache: " << cacheFile_ << std::endl;
  }
  close(fdWrite_);
  fdWrite_ = -1;
}

void WebPowerSwitchManager::findSwitches() {
  if (findSwitches_ == false) {
    return;
  }

  auto interface = getDefaultInterface();
  std::string ipAddress;
  std::string subNetMask;
  getIpAddressAndSubnetMask(interface, ipAddress, subNetMask);

  struct in_addr ipaddress;
  struct in_addr subnetmask;
  inet_pton(AF_INET, ipAddress.c_str(), &ipaddress);
  inet_pton(AF_INET, subNetMask.c_str(), &subnetmask);

  unsigned long firstIp = ntohl(ipaddress.s_addr & subnetmask.s_addr);
  unsigned long lastIp = ntohl(ipaddress.s_addr | ~(subnetmask.s_addr));
  std::vector<std::unique_ptr<WebPowerSwitch>> switches;
  CURLM* multi_handle = curl_multi_init();
  for (auto ip = firstIp; ip <= lastIp; ip++) {
    struct in_addr testIp = { htonl(ip) };
    if (verbose_ > 1) {
      std::cout << "ip: " << ip << " - " << htonl(ip) << " - " << inet_ntoa(testIp) << std::endl;
    }

    for (auto up : vUsernamePassword_) {
      std::unique_ptr<WebPowerSwitch> wps(new WebPowerSwitch(inet_ntoa(testIp)));
      wps->verbose(verbose_);
      wps->suppressDetectionErrors();
      CURL* request = wps->login(up.username, up.password);
      if (request == nullptr) {
        continue;
      }
      curl_multi_add_handle(multi_handle, request);
      switches.push_back(std::move(wps));
    }
  }

  // wait for requests to finish
  int still_running = 1;
  CURLMsg* msg;
  int msgs_left;
  while (still_running) {
    CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
    if (still_running) {
      mc = curl_multi_poll(multi_handle, nullptr, 0, 1000, nullptr);
    }
    if (mc) {
      std::cout << "=== mc: " << mc << std::endl;
      break;
    }
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
      if (msg->msg == CURLMSG_DONE) {
        for (auto& wps : switches) {
          if (msg->easy_handle == wps->handle()) {
            if (verbose_ > 2) {
              std::cout << "done: " << wps->host() << std::endl;
            }
            curl_multi_remove_handle(multi_handle, wps->handle());
            if (msg->data.result == CURLE_OK) {
              //std::cout << "looking good: " << wps->host() << std::endl;
              CURL* request = wps->next();
              if (request != nullptr) {
                curl_multi_add_handle(multi_handle, request);
              } else {
                // request == nullptr: must be done here
              }
              break;
            } else {
              // not CURLE_OK
              wps->logout();
            }
          }
        }
      }
    }
    // If there are still handles present, then continue checking for progress
    // maybe could be replaced with setting still_running above where handle is added.
    if (!still_running) {
      for (auto& wps : switches) {
        if (wps->handle() != nullptr) {
          still_running = 1;
          break;
        }
      }
    }
  }

  // Let them all finish completely.
  // (Althouygh I'm not sure this is necessary anymore.)
  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      for (auto& wps : switches) {
        if (msg->easy_handle == wps->handle()) {
          if (verbose_ > 2) {
            std::cout << "done: " << wps->host() << std::endl;
          }
          break;
        }
      }
    }
  }

  curl_multi_cleanup(multi_handle);
  multi_handle = nullptr;

  if (verbose_) {
    std::cout << "switches" << std::endl;
  }

  // parse the successes
  for (auto & wps : switches) {
    if (wps->isLoggedIn()) {
      if (verbose_) {
        std::cout << "host: " << wps->host() << " name: " << wps->name() << std::endl;
      }
      auto outletsCache = cache_[CACHE_KEY_OUTLETS];
      for (auto outlet : wps->outlets()) {
        if (verbose_ > 1) {
          std::cout << "outlet: " << outlet << std::endl;
        }
        outletsCache[outlet.name()][CACHE_OUTLETS_KEY_CONTROLLER] = wps->name();
        outletsCache[outlet.name()][CACHE_OUTLETS_KEY_ID] = outlet.id();
      }
      cache_[CACHE_KEY_CONTROLLERBYNAME][wps->name()][CACHE_CONTROLLERBYNAME_KEY_HOST] = wps->host();
      mNameToSwitch_[wps->name()].reset(wps.release());
    }
  }
}

std::string WebPowerSwitchManager::getDefaultInterface() {
  static const char* ROUTE_FILENAME = "/proc/net/route";
  std::fstream routes(ROUTE_FILENAME, std::ios::in);
  const size_t BUFSIZE = 1024;
  char buffer[BUFSIZE];
  while (routes.getline(buffer, BUFSIZE)) {
    std::istringstream iss(buffer);
    std::vector<std::string> pieces((std::istream_iterator<std::string>(iss)), std::istream_iterator<std::string>());
    if (std::stoul(pieces[1], nullptr, 16) == 0) {
      return pieces[0];
    }
  }
  return "";
} 

void WebPowerSwitchManager::getIpAddressAndSubnetMask(std::string interface, std::string& ipAddress, std::string& subNetMask) {
  struct ifaddrs* ifap;
  if (getifaddrs(&ifap) != 0) {
    std::cout << "getifaddrs failed: " << errno << " " << strerror(errno) << std::endl;
    return;
  }

  for (auto ifa = ifap; ifa; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    if (ifa->ifa_addr->sa_family != AF_INET) {
      continue;
    }
    if (strncmp(ifa->ifa_name, interface.c_str(), strlen(ifa->ifa_name)) == 0) {
      char extractedIpAddress[NI_MAXHOST];
      getnameinfo(ifa->ifa_addr,
        (ifa->ifa_addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
        extractedIpAddress, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      ipAddress = extractedIpAddress;
      auto sai = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_netmask);
      subNetMask = inet_ntoa(sai->sin_addr);
      break;
    }
  }

  freeifaddrs(ifap);
}
