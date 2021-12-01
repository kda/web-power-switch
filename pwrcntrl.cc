#include <cxxopts.hpp>
#include <iostream>
#include <strings.h>
#include <unistd.h>

#include "webpowerswitchmanager.h"


int main(int iArgc, char* szArgv[]) {
  cxxopts::Options options(szArgv[0], "pwrcntrl: control web power switches");
  options
    .positional_help("(all|switch_name|outlet_name) ([show]|on|off|toggle|cycle)")
    .show_positional_help();
  options
    .allow_unrecognised_options()
    .add_options()
      ("credentials", "provide pairs of username:password to use on switch(es).", cxxopts::value<std::vector<std::string>>())
      ("command", "[show]|on|off|toggle|cycle: show, turn on, turn off, toggle or cycle outlet.", cxxopts::value<std::string>())
      ("help", "show help")
      ("r,reset", "even if switch locations are known, go find them again", cxxopts::value<bool>()->default_value("false"))
      ("t,target", "'all', name of switch, name of outlet", cxxopts::value<std::string>())
      ("v,verbose", "increate verbosity of output")
    ;

  options.parse_positional({"target", "command"});
  auto optionsResult = options.parse(iArgc, szArgv);
  //std::cout << "parsed" << std::endl;

  std::string target;
  if (optionsResult.count("target")) {
    target = optionsResult["target"].as<std::string>();
  } else {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (optionsResult.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }
  //std::cout << "no help" << std::endl;


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

  // If 'all', then only implement show.
  if (target == "all") {
    wpsm->dumpSwitches(std::cout);
    return 0;
  }

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

  std::string command;
  if (optionsResult.count("command")) {
    command = optionsResult["command"].as<std::string>();
  } else {
    // no command
    return 0;
  }

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
    std::cout << "ERROR: unrecognized command: " << command << std::endl;
    return 0;
  }
  std::cout << wps->name() << ": " << *(wps->getOutlet(target)) << std::endl;

  return 0;
}
