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
  vUsernamePassword_.push_back({std::string(username), std::string(password)});
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
  if (verbose_ > 2) {
    std::cerr << "DEBUG: WebPowerSwitchManager::getSwitch(" << name << ", "
              << allow_miss << ") called" << std::endl;
  }
  if (load() == false) {
    if (verbose_ > 3) {
      std::cerr << "DEBUG: load() failed in getSwitch" << std::endl;
    }
    return nullptr;
  }
  auto iter = mNameToSwitch_.find(name);
  if (iter != mNameToSwitch_.end()) {
    return iter->second.get();
  }
  if (!cache_[CACHE_KEY_CONTROLLERBYNAME][name]) {
    if (allow_miss == false) {
      std::cerr << "ERROR: unknown switch name: " << name << std::endl;
    }
    return nullptr;
  }
  return connectSwitch(cache_[CACHE_KEY_CONTROLLERBYNAME][name][CACHE_CONTROLLERBYNAME_KEY_HOST].as<std::string>());
}

WebPowerSwitch* WebPowerSwitchManager::getSwitchByIp(std::string ip, bool allow_miss) {
  if (verbose_ > 2) {
    std::cerr << "DEBUG: WebPowerSwitchManager::getSwitchByIp(" << ip << ", "
              << allow_miss << ") called" << std::endl;
  }
  if (load() == false) {
    if (verbose_ > 3) {
      std::cerr << "DEBUG: load() failed in getSwitchByIp" << std::endl;
    }
    return nullptr;
  }

  // Search cache
  if (cache_[CACHE_KEY_CONTROLLERBYNAME]) {
    for (auto controller : cache_[CACHE_KEY_CONTROLLERBYNAME]) {
      if (verbose_ > 1) {
        std::cerr << "DEBUG: controller name: "
                  << controller.first.as<std::string>() << " host: "
                  << controller.second[CACHE_CONTROLLERBYNAME_KEY_HOST]
                         .as<std::string>()
                  << std::endl;
      }
      if (controller.second[CACHE_CONTROLLERBYNAME_KEY_HOST].as<std::string>() == ip) {
        return getSwitch(controller.first.as<std::string>(), allow_miss);
      }
    }
  }

  // Open based on IP
  return connectSwitch(ip);
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
  if (verbose_ > 2) {
    std::cerr << "DEBUG: WebPowerSwitchManager::dumpSwitches called" << std::endl;
  }
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
    if (verbose_ > 2) {
      std::cerr << "DEBUG: enableCache_: " << enableCache_ << std::endl;
    }
    return;
  }
  if (resetCache_) {
    if (verbose_ > 2) {
      std::cerr << "DEBUG: resetCache_: " << resetCache_ << std::endl;
    }
    resetCache_ = false;
    return;
  }
  if (validateCacheFile() == false) {
    if (verbose_ > 2) {
      std::cerr << "DEBUG: validateCacheFile failed" << std::endl;
    }
    return;
  }
  struct stat statCacheFile;
  auto statRetval = stat(cacheFile_.c_str(), &statCacheFile);
  if (statRetval != 0) {
    if (verbose_ > 2) {
      std::cerr << "DEBUG: cache file does not exist (or is not readable): "
                << cacheFile_ << std::endl;
    }
    return;
  }
  auto tooOld = time(nullptr) - cacheTimeout_;
  if (statCacheFile.st_mtime <= tooOld) {
    if (verbose_ > 2) {
      std::cerr << "DEBUG: cache file is too old : " << cacheFile_ << std::endl;
    }
    return;
  }

  auto fd = open(cacheFile_.c_str(), O_RDONLY);
  if (fd < 0) {
    std::cerr << "ERROR: failed to open cache: " << cacheFile_ << std::endl;
    return;
  }
  auto r = flock(fd, LOCK_SH | LOCK_NB);
  if (r < 0) {
    std::cerr << "ERROR: failed to obtain read lock: " << cacheFile_ << " ("
              << errno << ": " << strerror(errno) << ")" << std::endl;
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
    std::cerr << "ERROR: failed to load cache: " << cacheFile_ << std::endl;
  }
  if (verbose_ > 2) {
    std::cerr << "DEBUG: isCacheLoaded(): " << isCacheLoaded() << std::endl;
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
  if (fdWrite_ < 0) {
    std::cerr << "ERROR: failed to open for writing cache file: " << cacheFile_
              << " (" << errno << ": " << strerror(errno) << ")" << std::endl;
    return;
  }
  auto r = flock(fdWrite_, LOCK_EX | LOCK_NB);
  if (r < 0) {
    std::cerr << "ERROR: failed to obtain write lock: " << cacheFile_ << " ("
              << errno << ": " << strerror(errno) << ")" << std::endl;
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
      CURL* request = wps->startLogin(up.username, up.password);
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
    addSwitchToCache(std::move(wps));
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
    if (strncmp(ifa->ifa_name, interface.data(), strlen(ifa->ifa_name)) == 0) {
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

WebPowerSwitch* WebPowerSwitchManager::connectSwitch(std::string ip) {
  auto wps = std::make_unique<WebPowerSwitch>(ip);
  wps->verbose(verbose_);
  for (auto up : vUsernamePassword_) {
    if (wps->login(up.username, up.password)) {
      break;
    }
  }
  if (wps->isLoggedIn() == false) {
    if (verbose_ > 0) {
      std::cerr << "ERROR: login failed switch ip: " << ip << std::endl;
    }
    return nullptr;
  }

  // Store the name, so the pointer can be sent back from the map.
  std::string name(wps->name());
  writeCacheStart();
  addSwitchToCache(std::move(wps));
  writeCacheFinish();
  return mNameToSwitch_.find(name)->second.get();
}

void WebPowerSwitchManager::addSwitchToCache(std::unique_ptr<WebPowerSwitch>&& wps) {
  if (wps->isLoggedIn()) {
    if (verbose_) {
      std::cout << "host: " << wps->host() << " name: " << wps->name() << std::endl;
    }
    auto outletsCache = cache_[CACHE_KEY_OUTLETS];
    for (auto outlet : wps->outlets()) {
      if (verbose_ > 1) {
        std::cout << "outlet: " << outlet << std::endl;
      }
      outletsCache[std::string(outlet.name())][CACHE_OUTLETS_KEY_CONTROLLER] = std::string(wps->name());
      outletsCache[std::string(outlet.name())][CACHE_OUTLETS_KEY_ID] = outlet.id();
    }
    cache_[CACHE_KEY_CONTROLLERBYNAME][std::string(wps->name())][CACHE_CONTROLLERBYNAME_KEY_HOST] = std::string(wps->host());
    mNameToSwitch_[std::string(wps->name())].reset(wps.release());
  }
}
