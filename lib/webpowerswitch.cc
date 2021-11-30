#include "webpowerswitch.h"

#include <iomanip>
#include <stdlib.h>
#include <iostream>
#include <openssl/md5.h>
#include <sstream>
#include <tidy/tidybuffio.h>
#include <unordered_map>

#include "tidyHelper.h"
#include "tidydocwrapper.h"
#include "trim.h"


const long WebPowerSwitch::CURL_TIMEOUT = 10L;

std::string generatePostData(std::unordered_map<std::string, std::string>& fields) {
  std::ostringstream os;
  for (auto field : fields) {
    os << field.first << "=" << field.second << "&";
  }
  os.seekp(-1, std::ios_base::end);
  os << std::ends;
  return os.str();
}

size_t writeFunctionStream(char* ptr, size_t size, size_t nmemb,
                           std::ostream* ostr) {
  
  auto totalBytes = size * nmemb;
  ostr->write(ptr, totalBytes);
  return totalBytes;
}

WebPowerSwitch::WebPowerSwitch(std::string host)
  : host_(host) {
}

WebPowerSwitch::~WebPowerSwitch() {
  logout();
}

bool WebPowerSwitch::connect() {
  if (isConnected()) {
    return true;
  }

  return false;
}

bool WebPowerSwitch::login(std::string username, std::string password) {
  if (isLoggedIn()) {
    return true;
  }

  // Create Share
  share_ = curl_share_init();

  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

  // Fetch
  CURL* request;
  request = curl_easy_init();

  std::ostringstream os;
  curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, writeFunctionStream);
  curl_easy_setopt(request, CURLOPT_WRITEDATA, &os);
  char curlErrbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(request, CURLOPT_ERRORBUFFER, curlErrbuf);

  curl_easy_setopt(request, CURLOPT_URL, (prefix_ + host()).c_str());
  curl_easy_setopt(request, CURLOPT_TIMEOUT, CURL_TIMEOUT);
  if (verboseCurl_) {
    curl_easy_setopt(request, CURLOPT_VERBOSE, 1L);
  }
  //std::cout << "pre-fetch login page" << std::endl;
  auto curlResult = curl_easy_perform(request);
  //std::cout << "post-fetch login page" << std::endl;
  if (curlResult != CURLE_OK) {
    if (curlResult == CURLE_OPERATION_TIMEDOUT) {
      std::cerr << "ERROR: timed out host(): " << host() << std::endl;
      return false;
    }
    if (!suppressDetectionErrors_) {
      std::cerr << "host(): " << host() << " error (" << curlResult
                << "): " << curl_easy_strerror(curlResult) << std::endl;
      std::cerr << "curlErrbuf: " << curlErrbuf << std::endl;
    }
    return false;
  }

  //std::cout << "fetched content:" << std::endl;
  //std::cout << os.str() << std::endl;

  // Parse
  TidyDocWrapper tdw;
  TidyBuffer errbuf = {0};
  auto result = tidySetErrorBuffer(tdw, &errbuf);
  if (result != 0) {
    std::cerr << "failed to set error buffer: " << result << std::endl;
    return false;
  }
  result = tidyOptSetInt(tdw, TidyUseCustomTags, TidyCustomBlocklevel);
  if (result == false) {
    std::cerr << "ERROR: tidyOptSetInt(tdw, TidyUseCustomTags, "
                 "TidyCustomBlocklevel) failed: "
              << result << " errbuf: " << errbuf.bp << std::endl;
    return false;
  }
  result = tidyParseString(tdw, os.str().c_str());
  if (result > 1) {
    std::cerr << "ERROR: (login) failed to parse: " << result
              << " errbuf: " << errbuf.bp << std::endl;
    return false;
  }

  // Find Challenge value
  TidyNode inputNode = tidyHelper::findNodeByAttr(tdw, "input", TidyAttr_NAME, "challenge", nullptr);
  if (inputNode == nullptr) {
    if (!suppressDetectionErrors_) {
      std::cerr << "ERROR: host(): " << host() << " failed to find input" << std::endl;
    }
    return false;
  }
  //std::cout << "name: " << tidyNodeGetName(inputNode) << std::endl;
  auto value = tidyAttrGetById(inputNode, TidyAttr_VALUE);
  if (value == nullptr) {
    std::cerr << "ERROR: failed to find challenge value" << std::endl;
    return false;
  }
  std::string challenge = tidyAttrValue(value);
  //std::cout << "challenge: " << challenge << std::endl;

  // determine form action
  TidyNode formNode = tidyHelper::findNodeByAttr(tdw, "form", TidyAttr_NAME, "login", nullptr);
  if (formNode == nullptr) {
    std::cerr << "ERROR: failed to find form" << std::endl;
    return false;
  }
  value = tidyAttrGetById(formNode, TidyAttr_ACTION);
  if (value == nullptr) {
    std::cerr << "ERROR: failed to find action value" << std::endl;
    return false;
  }
  std::string action = tidyAttrValue(value);
  //std::cout << "action: " << action << std::endl;

  value = tidyAttrGetById(formNode, TidyAttr_METHOD);
  if (value == nullptr) {
    std::cerr << "ERROR: failed to find method value" << std::endl;
    return false;
  }
  //std::string method = tidyAttrValue(value);
  //std::cout << "method: " << method << std::endl;

  // create password
  auto passphrase = challenge + username + password + challenge;
  //std::cout << "passphrase: " << passphrase << std::endl;
  unsigned char md5digest[MD5_DIGEST_LENGTH + 1];
  MD5(reinterpret_cast<const unsigned char *>(passphrase.c_str()), passphrase.length(), md5digest);
  std::ostringstream osDigest;
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    osDigest << std::setfill('0') << std::setw(2) << std::hex << (int)md5digest[i];
  }
  //std::cout << "md5digest: " << osDigest.str() << std::endl;
  std::unordered_map<std::string, std::string> postFields;
  postFields["Username"] = username;
  postFields["Password"] = osDigest.str();

  // Post processing
  os.seekp(0);
  os.str("");

  curl_easy_setopt(request, CURLOPT_URL, (prefix_ + host() + action).c_str());
  curl_easy_setopt(request, CURLOPT_HEADER, 1L);
  curl_easy_setopt(request, CURLOPT_COOKIEFILE, "");
  curl_easy_setopt(request, CURLOPT_SHARE, share_);
  curl_easy_setopt(request, CURLOPT_TIMEOUT, CURL_TIMEOUT);
  if (verboseCurl_) {
    curl_easy_setopt(request, CURLOPT_VERBOSE, 1L);
  }

  auto postData = generatePostData(postFields);
  //std::cout << "postData: " << postData << std::endl;
  //std::cout << "postData.length(): " << postData.length() << std::endl;
  curl_easy_setopt(request, CURLOPT_POSTFIELDS, postData.c_str());

  //std::cout << "pre-fetch cookie page" << std::endl;
  curlResult = curl_easy_perform(request);
  //std::cout << "post-fetch cookie page" << std::endl;
  if (verbose_) {
    std::cout << "curlResult: " << curlResult << std::endl;
  }

  if (verbose_) {
    std::cout << "response content:" << std::endl << os.str() << std::endl;
  }

  long responseCode;
  curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &responseCode);
  //std::cout << "host(): " << host() << " responseCode:" << responseCode << std::endl;
  if (responseCode == 200) {
    loggedIn_ = true;
    username_ = username;
    password_ = password;
  } else {
    if (!suppressDetectionErrors_) {
      std::cout << "host(): " << host() << " responseCode:" << responseCode << std::endl;
    }
  }

  curl_easy_cleanup(request);
  buildOutlets(true);

  return loggedIn_;
}

void WebPowerSwitch::logout() {
  connected_ = false;
  loggedIn_ = false;
  if (share_ != nullptr) {
    curl_share_cleanup(share_);
    share_ = nullptr;
  }
}

void WebPowerSwitch::buildOutlets(bool force) {
  if (isLoggedIn() == false) {
    std::cerr << "not logged in" << std::endl;
    return;
  }
  if (force == false && nextBuild_ > time(nullptr)) {
    return;
  }
  name_.clear();
  outlets_.clear();

  CURL* request;
  request = curl_easy_init();
  curl_easy_setopt(request, CURLOPT_URL, (prefix_ + host() + "/index.htm").c_str());
  //curl_easy_setopt(request, CURLOPT_URL, (prefix_ + host() + "/").c_str());
  curl_easy_setopt(request, CURLOPT_SHARE, share_);
  curl_easy_setopt(request, CURLOPT_COOKIEFILE, "");
  curl_easy_setopt(request, CURLOPT_TIMEOUT, CURL_TIMEOUT);
  if (verboseCurl_) {
    curl_easy_setopt(request, CURLOPT_VERBOSE, 1L);
  }
  //curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, 1L);

  std::ostringstream os;
  curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, writeFunctionStream);
  curl_easy_setopt(request, CURLOPT_WRITEDATA, &os);


  //std::cout << "pre-fetch status page" << std::endl;
  auto curlResult = curl_easy_perform(request);
  //std::cout << "curlResult: " << curlResult << std::endl;
  //std::cout << "post-fetch status page" << std::endl;

  if (verbose_) {
    std::cout << "fetched content:" << std::endl;
    std::cout << os.str() << std::endl;
  }

  TidyDocWrapper tdw;
  TidyBuffer errbuf = {0};
  auto result = tidySetErrorBuffer(tdw, &errbuf);
  if (result != 0) {
    std::cerr << "failed to set error buffer: " << result << std::endl;
    return;
  }
  result = tidyParseString(tdw, os.str().c_str());
  if (result > 1) {
    std::cerr << "(buildOutlets) failed to parse: " << result << std::endl;
    std::cerr << "errbuf: " << errbuf.bp << std::endl;
    return;
  }

  // find name
  auto node = tidyHelper::findNodeByContent(tdw, "th", "Controller: ", nullptr);
  if (node == nullptr) {
    std::cerr << "findNodeByContent failed (" << host() << ") 'th' 'Controller: '" << std::endl;
    return;
  }
  TidyBuffer tbuf = {0};
  tidyNodeGetText(tdw, node, &tbuf);
  //if (tidyNodeGetText(tdw, node, &tbuf) == false) {
    //std::cerr << "tidyNodeGetText failed (" << host() << "). errbuf: " << errbuf.bp << std::endl;
  //  std::cerr << "tidyNodeGetText failed (" << host() << "): " << tbuf.bp << std::endl;
  //  return;
  //}
  if (tbuf.bp == nullptr) {
    std::cerr << "tidyNodeGetText failed (" << host() << ") tbuf.bp == nullptr" << std::endl;
    return;
  }
  name_ = trim(reinterpret_cast<char *>(tbuf.bp));
  name_ = name_.substr(name_.find(':') + 2);

  // find individual control
  node = tidyHelper::findNodeByContent(tdw, "td", "Individual Control", nullptr);
  auto tr = tidyGetParent(tidyGetParent(node));
  tr = tidyHelper::findNode(tdw, "tr", tr);
  tr = tidyHelper::findNode(tdw, "tr", tr);
  while (tr) {
    Outlet outlet;

    auto td = tidyGetChild(tr);
    auto number = tidyGetChild(td);
    TidyBuffer tbuf = {0};
    tidyNodeGetText(tdw, number, &tbuf);
    int outletId = atoi(reinterpret_cast<char *>(tbuf.bp));

    td = tidyGetNext(td);
    auto name = tidyGetChild(td);
    tidyBufClear(&tbuf);
    tidyNodeGetText(tdw, name, &tbuf);
    //ostr << "name: " << tbuf.bp;
    std::string outletName = reinterpret_cast<char *>(tbuf.bp);
    outletName = trim(outletName);

    td = tidyGetNext(td);
    auto state = tidyGetChild(td);
    while (tidyNodeGetType(state) != TidyNode_Text) {
      state = tidyGetChild(state);
    }
    tidyBufClear(&tbuf);
    tidyNodeGetText(tdw, state, &tbuf);
    //ostr << "state: " << tbuf.bp;
    OutletState outletState = OUTLET_STATE_UNKNOWN;
    if (strncmp("ON", reinterpret_cast<char *>(tbuf.bp), 2) == 0) {
      outletState = OUTLET_STATE_ON;
    } else if (strncmp("OFF", reinterpret_cast<char *>(tbuf.bp), 3) == 0) {
      outletState = OUTLET_STATE_OFF;
    }

    outlets_.push_back({outletId, outletName, outletState});

    tr = tidyGetNext(tr);
  }

  nextBuild_ = time(nullptr) + 60;
}

void WebPowerSwitch::dumpOutlets(std::ostream& ostr) {
  buildOutlets();
  ostr << "controller: " << name_ << std::endl;
  ostr << " #  State  Name" << std::endl;
  for (auto outlet : outlets_) {
    ostr << outlet << std::endl;
  }
}

Outlet* WebPowerSwitch::getOutlet(const std::string& name) {
  buildOutlets();
  for (auto outletIter = outlets_.begin(); outletIter != outlets_.end(); outletIter++) {
    if (outletIter->name() == name) {
      return &(*outletIter);
    }
  }
  return nullptr;
}

bool WebPowerSwitch::on(const std::string& outletName) {
  auto ol = getOutlet(outletName);
  if (ol->state() == OUTLET_STATE_ON) {
    return true;
  }
  return setState(ol, OUTLET_STATE_ON);
}

bool WebPowerSwitch::off(const std::string& outletName) {
  auto ol = getOutlet(outletName);
  if (ol->state() == OUTLET_STATE_OFF) {
    return true;
  }
  return setState(ol, OUTLET_STATE_OFF);
}

bool WebPowerSwitch::toggle(const std::string& outletName) {
  auto ol = getOutlet(outletName);
  return setState(ol, ol->state() == OUTLET_STATE_ON ? OUTLET_STATE_OFF : OUTLET_STATE_ON);
}

bool WebPowerSwitch::setState(const Outlet* outlet, OutletState newState) {
  if (isLoggedIn() == false) {
    std::cerr << "not logged in" << std::endl;
    return false;
  }
  std::ostringstream ostrUrl;
  ostrUrl << prefix_ << host() << "/outlet?" << outlet->id() << "=";
  switch (newState) {
  case OUTLET_STATE_ON:
    ostrUrl << "ON";
    break;
  case OUTLET_STATE_OFF:
    ostrUrl << "OFF";
    break;
  case OUTLET_STATE_UNKNOWN:
    std::cerr << "unknown new state" << std::endl;
    return false;
  }

  CURL* request;
  request = curl_easy_init();
  
  curl_easy_setopt(request, CURLOPT_URL, ostrUrl.str().c_str());
  curl_easy_setopt(request, CURLOPT_SHARE, share_);
  curl_easy_setopt(request, CURLOPT_COOKIEFILE, "");
  curl_easy_setopt(request, CURLOPT_TIMEOUT, CURL_TIMEOUT);
  if (verboseCurl_) {
    curl_easy_setopt(request, CURLOPT_VERBOSE, 1L);
  }

  std::ostringstream os;
  curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, writeFunctionStream);
  curl_easy_setopt(request, CURLOPT_WRITEDATA, &os);

  auto result = curl_easy_perform(request);

  buildOutlets(true);
  return result == CURLE_OK;
}
