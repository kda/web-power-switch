#include <fstream>
#include <unistd.h>

#include "cxxopts.hpp"
#include "webpowerswitchmanager.h"

// must persist beyond life of method
static std::vector<std::string> arguments;

std::vector<char*> loadCommandLineArguments(int iArgc, char* szArgv[], std::string optionsFilename) {
  std::vector<char*> cla;
  for (int iArg = 0; iArg < iArgc; iArg++, szArgv++) {
    cla.push_back(*szArgv);
  }

  std::ifstream optionsFile(optionsFilename);
  if (optionsFile.good()) {
    const std::regex wsRE("\\s+");
    arguments.clear();
    for (std::string line; std::getline(optionsFile, line); ) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::copy(
        std::sregex_token_iterator(line.begin(), line.end(), wsRE, -1),
        std::sregex_token_iterator(),
        std::back_inserter<std::vector<std::string>>(arguments));
    }
    for (const auto& argument : arguments) {
      cla.push_back(const_cast<char*>(argument.c_str()));
    }
  }

  return cla;
}


int main(int iArgc, char* szArgv[]) {
  cxxopts::Options options(szArgv[0], "pwrcntrl: control web power switches");

  std::string optionsFilename(getenv("HOME"));
  optionsFilename += "/.pwrcntrlrc";

  options
    .positional_help("(all|switch_name|outlet_name) ([show]|on|off|toggle|cycle)")
    .show_positional_help();
  options
    .allow_unrecognised_options()
    .add_options()
      ("command", "show|on|off|toggle|cycle: show, turn on, turn off, toggle or cycle outlet.", cxxopts::value<std::string>())
      ("credentials", "provide pairs of username:password to use on switch(es).", cxxopts::value<std::vector<std::string>>())
      ("help", "show help")
      ("options", "<option_file>: read command line parameters from option_file.", cxxopts::value<std::string>()->default_value(optionsFilename))
      ("r,reset", "even if switch locations are known, go find them again.")
      ("t,target", "'all'|<name_of_switch|name_of_outlet", cxxopts::value<std::string>())
      ("v,verbose", "increate verbosity of output")
    ;

  options.parse_positional({"target", "command"});
  auto commandLineArguments = loadCommandLineArguments(iArgc, szArgv, optionsFilename);
  auto optionsResult = options.parse(commandLineArguments.size(), &commandLineArguments[0]);
  if (optionsResult.count("options") && optionsResult["options"].as<std::string>() != optionsFilename) {
    commandLineArguments = loadCommandLineArguments(iArgc, szArgv, optionsResult["options"].as<std::string>());
    optionsResult = options.parse(commandLineArguments.size(), &commandLineArguments[0]);
  }

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

  if (optionsResult.count("verbose") > 1) {
    for (auto argument : commandLineArguments) {
      std::cout << "commandLineArguments: " << argument << std::endl;
    }
  }

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

  auto wps = wpsm->getSwitch(target, true);
  if (wps != nullptr) {
    wps->dumpOutlets(std::cout);
    return 0;
  }

  wps = wpsm->getSwitchByOutletName(target);
  if (wps == nullptr) {
    std::cout << "unknown outlet (or switch): " << target << std::endl;
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
