#include <cxxopts.hpp>
#include <iostream>
#include <strings.h>
#include <unistd.h>

#include "webpowerswitchmanager.h"


int main(int iArgc, char* szArgv[]) {
  cxxopts::Options options(szArgv[0], "control web power switches");
  options
    .positional_help("([show]|on|off|toggle|cycle)")
    .show_positional_help();
  options
    .allow_unrecognised_options()
    .add_options()
      ("a,all", "show all outlets (even if something other than 'show' is specified).", cxxopts::value<bool>())
      ("credentials", "provide pairs of username:password to use on switch(es).", cxxopts::value<std::vector<std::string>>())
      ("command", "[show]|on|off|toggle|cycle: show, turn on, turn off, toggle or cycle outlet.", cxxopts::value<std::vector<std::string>>())
      ("d,discover", "if switch name is not known, then attempt to discover it")
      ("h,host", "name of host (supports IP, as well)")
      ("help", "show help")
      ("o,outlet", "name of outlet")
      ("r,reset", "even if switch locations are known, go find them again")
      ("s,switch", "name of switch")
      ("v,verbose", "increate verbosity of output")
    ;

  options.parse_positional({"command"});
  auto optionsResult = options.parse(iArgc, szArgv);
  //std::cout << "parsed" << std::endl;

  if (optionsResult.count("help") ||
      (optionsResult.count("command") &&
       optionsResult["command"].as<std::vector<std::string>>()[0] == "help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }
  //std::cout << "no help" << std::endl;

  //cxxopts::value<std::string>()->default_value("show");
  std::string command = {"unknown"};
  if (optionsResult.count("command") != 0) {
    command = optionsResult["command"].as<std::vector<std::string>>()[0];
  }
  //std::cout << "command: " << command << std::endl;

  std::unique_ptr<WebPowerSwitchManager> wpsm(new WebPowerSwitchManager);

  // Add credentials
  if (optionsResult.count("credentials") != 0) {
    for (const auto& credential : optionsResult["credentials"].as<std::vector<std::string>>()) {
      //std::cout << "credential: " << credential << std::endl;
      auto separator = credential.find(":");
      if (separator == std::string::npos) {
        std::cerr << "Invalid credential missing separator ':' (" << credential << ")" << std::endl;
        return -1;
      }
      auto username = credential.substr(0, separator);
      auto password = credential.substr(separator + 1);
      //std::cout << "username: " << username << " password: " << password << std::endl;
      if (!wpsm->addUsernamePassword(username, password)) {
        std::cerr << "Failed to add username (" << username << ") and password (" << password << ")." << std::endl;
        return -1;
      }
    }
  }

  // Verbosity
  if (optionsResult.count("verbose") != 0) {
    wpsm->verbose(optionsResult.count("verbose"));
  }

  if (optionsResult.count("reset") != 0) {
    wpsm->resetCache();
  }

  //std::cout << "target: -" << target << "-" << std::endl;

  // If 'all', then only implement show.
  if (optionsResult.count("all")) {
    if (optionsResult["all"].as<bool>()) {
      wpsm->dumpSwitches(std::cout);
      return 0;
    }
  }
#ifdef kda_COMMENTED_OUT
  auto wps = wpsm->getSwitch(target);
  if (wps != nullptr) {
    wps->dumpOutlets(std::cout);
    return 0;
  }

  wps = wpsm->getSwitchByOutletName(target);
  if (wps == nullptr) {
    std::cout << "unknown outlet: " << target << std::endl;
    return -1;
  }

  std::cout << wps->name() << ": " << *(wps->getOutlet(target)) << std::endl;

  if (strncasecmp(command.c_str(), "on", 2) == 0) {
    wps->on(target);
  } else if (strncasecmp(command.c_str(), "off", 3) == 0) {
    wps->off(target);
  } else if (strncasecmp(command.c_str(), "cycle", 5) == 0) {
    wps->toggle(target);
    std::cout << wps->name() << ": " << *(wps->getOutlet(target)) << std::endl;
    sleep(5);
    wps->toggle(target);
  } else if (strncasecmp(command.c_str(), "toggle", 5) == 0) {
    wps->toggle(target);
  } else {
    return 0;
  }
  std::cout << wps->name() << ": " << *(wps->getOutlet(target)) << std::endl;
#endif /* kda_COMMENTED_OUT */

  return 0;
}
