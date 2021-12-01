#include "webpowerswitch.h"

#include <iomanip>
#include <stdlib.h>
#include <iostream>
#include <openssl/md5.h>
#include <sstream>
#include <tidy/tidybuffio.h>
#include <unordered_map>
#include <unistd.h>

#include "tidyHelper.h"
#include "tidydocwrapper.h"
#include "trim.h"


const long WebPowerSwitch::CURL_TIMEOUT = 5L;

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

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size)
{
  size_t i;
  size_t c;
  unsigned int width=0x10;
 
  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
          text, (long)size, (long)size);
 
  for(i=0; i<size; i+= width) {
    fprintf(stream, "%4.4lx: ", (long)i);
 
    /* show hex to the left */
    for(c = 0; c < width; c++) {
      if(i+c < size)
        fprintf(stream, "%02x ", ptr[i+c]);
      else
        fputs("   ", stream);
    }
 
    /* show data on the right */
    for(c = 0; (c < width) && (i+c < size); c++) {
      char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
      fputc(x, stream);
    }
 
    fputc('\n', stream); /* newline */
  }
}
 
static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  const char *text;
  (void)handle; /* prevent compiler warning */
  (void)userp;
 
  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */
    return 0;
 
  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }
 
  dump(text, stderr, (unsigned char *)data, size);
  return 0;
}

WebPowerSwitch::WebPowerSwitch(std::string host)
  : host_(host) {
}

WebPowerSwitch::~WebPowerSwitch() {
  logout();
}

CURL* WebPowerSwitch::login(std::string username, std::string password) {
  if (loggedIn_) {
    return nullptr;
  }
  if (state_ != STATE_UNINITIALIZED) {
    return nullptr;
  }
  if (request_ != nullptr) {
    return nullptr;
  }

  username_ = username;
  password_ = password;

  // Create Share
  share_ = curl_share_init();

  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

  // Fetch
  initializeRequest();

  curl_easy_setopt(request_, CURLOPT_URL, (prefix_ + host()).c_str());
  //curl_easy_setopt(request_, CURLOPT_HEADER, 1L);
  curl_easy_setopt(request_, CURLOPT_TIMEOUT, CURL_TIMEOUT);
  if (verboseCurl_) {
    curl_easy_setopt(request_, CURLOPT_VERBOSE, 1L);
  }
  state_ = STATE_INITIAL_PAGE_REQUESTED;
  return request_;
}

CURL* WebPowerSwitch::next() {
  if (verbose_) {
    std::cout << "WebPowerSwitch::next state_: " << state_ << std::endl;
  }
  switch (state_) {
  case STATE_UNINITIALIZED:
    clearRequest();
    return nullptr;
  case STATE_INITIAL_PAGE_REQUESTED:
    {
		dumpCookies();
    clearRequest();
    // Parse
    TidyDocWrapper tdw;
    TidyBuffer errbuf = {0};
    auto result = tidySetErrorBuffer(tdw, &errbuf);
    if (result != 0) {
      std::cerr << "failed to set error buffer: " << result << std::endl;
      return nullptr;
    }
    result = tidyOptSetInt(tdw, TidyUseCustomTags, TidyCustomBlocklevel);
    if (result == false) {
      std::cerr << "tidyOptSetInt(tdw, TidyUseCustomTags, TidyCustomBlocklevel) failed: " << result << std::endl;
      std::cerr << "errbuf: " << errbuf.bp << std::endl;
      return nullptr;
    }
    //std::cout << "os_: " << os_.str() << std::endl;
    result = tidyParseString(tdw, os_.str().c_str());
    if (result > 1) {
      std::cerr << "(login) failed to parse: " << result << std::endl;
      std::cerr << "errbuf: " << errbuf.bp << std::endl;
      return nullptr;
    }

    // Find Challenge value
    TidyNode inputNode = tidyHelper::findNodeByAttr(tdw, "input", TidyAttr_NAME, "challenge", nullptr);
    if (inputNode == nullptr) {
      if (!suppressDetectionErrors_) {
        std::cerr << "host(): " << host() << " failed to find input" << std::endl;
      }
      return nullptr;
    }
    //std::cout << "name: " << tidyNodeGetName(inputNode) << std::endl;
    auto value = tidyAttrGetById(inputNode, TidyAttr_VALUE);
    if (value == nullptr) {
      std::cerr << "failed to find challenge value" << std::endl;
      return nullptr;
    }
    std::string challenge = tidyAttrValue(value);
    //std::cout << "challenge: " << challenge << std::endl;

    // determine form action
    TidyNode formNode = tidyHelper::findNodeByAttr(tdw, "form", TidyAttr_NAME, "login", nullptr);
    if (formNode == nullptr) {
      std::cerr << "failed to find form" << std::endl;
      return nullptr;
    }
    value = tidyAttrGetById(formNode, TidyAttr_ACTION);
    if (value == nullptr) {
      std::cerr << "failed to find action value" << std::endl;
      return nullptr;
    }
    std::string action = tidyAttrValue(value);
    //std::cout << "action: " << action << std::endl;

    // create password
    auto passphrase = challenge + username_ + password_ + challenge;
    //std::cout << "passphrase: " << passphrase << std::endl;
    unsigned char md5digest[MD5_DIGEST_LENGTH + 1];
    MD5(reinterpret_cast<const unsigned char *>(passphrase.c_str()), passphrase.length(), md5digest);
    std::ostringstream osDigest;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
      osDigest << std::setfill('0') << std::setw(2) << std::hex << (int)md5digest[i];
    }
    //std::cout << "md5digest: " << osDigest.str() << std::endl;
    std::unordered_map<std::string, std::string> postFields;
    postFields["Username"] = username_;
    postFields["Password"] = osDigest.str();

    initializeRequest();

    curl_easy_setopt(request_, CURLOPT_URL, (prefix_ + host() + action).c_str());
    curl_easy_setopt(request_, CURLOPT_HEADER, 1L);
    curl_easy_setopt(request_, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(request_, CURLOPT_SHARE, share_);
    curl_easy_setopt(request_, CURLOPT_TIMEOUT, CURL_TIMEOUT);
    if (verboseCurl_) {
      curl_easy_setopt(request_, CURLOPT_VERBOSE, 1L);
    }
    curl_easy_setopt(request_, CURLOPT_FOLLOWLOCATION, 1L);

    auto postData = generatePostData(postFields);
    //std::cout << "postData: " << postData << std::endl;
    //std::cout << "postData.length(): " << postData.length() << std::endl;
    curl_easy_setopt(request_, CURLOPT_COPYPOSTFIELDS, postData.c_str());
    state_ = STATE_LOGIN_REQUESTED;
		//usleep(1000000);
    }
    return request_;
  case STATE_LOGIN_REQUESTED:
    dumpCookies();
    long responseCode;
    curl_easy_getinfo(request_, CURLINFO_RESPONSE_CODE, &responseCode);
    //std::cout << "host(): " << host() << " responseCode: " << responseCode << std::endl;
    clearRequest();
    if (responseCode == 200) {
      loggedIn_ = true;
      prepToFetchOutlets();
      return request_;
    } else {
      state_ = STATE_LOGIN_FAILED;
      if (!suppressDetectionErrors_) {
        std::cout << "host(): " << host() << " responseCode: " << responseCode << std::endl;
        std::cout << "os_: " << os_.str() << std::endl;
      }
    }
    break;
  case STATE_LOGGED_IN:
    {
    dumpCookies();
    clearRequest();
    //std::cout << "host(): " << host() << std::endl;
    //std::cout << "os_: " << os_.str() << std::endl;

    TidyDocWrapper tdw;
    TidyBuffer errbuf = {0};
    auto result = tidySetErrorBuffer(tdw, &errbuf);
    if (result != 0) {
      std::cerr << "failed to set error buffer: " << result << std::endl;
      return nullptr;
    }
    result = tidyParseString(tdw, os_.str().c_str());
    if (result > 1) {
      std::cerr << "(buildOutlets) failed to parse: " << result << std::endl;
      std::cerr << "errbuf: " << errbuf.bp << std::endl;
      return nullptr;
    }

    // find name
    auto node = tidyHelper::findNodeByContent(tdw, "th", "Controller: ", nullptr);
    if (node == nullptr) {
      std::cerr << "findNodeByContent failed (" << host() << ") 'th' 'Controller: '" << std::endl;
      return nullptr;
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
      return nullptr;
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

      auto tdNext = tidyGetNext(td);
      if (tdNext == nullptr) {
        std::cerr << "tidyGetNext failed (" << host() << ") td: " << td << std::endl;
        tr = tidyGetNext(tr);
        continue;
      }
      td = tdNext;
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
    state_ = STATE_OUTLETS_BUILT;
    }
    break;
  default:
    break;
  }

  return nullptr;
}

void WebPowerSwitch::logout() {
  state_ = STATE_UNINITIALIZED;
  loggedIn_ = false;
  clearRequest();
  if (share_ != nullptr) {
    curl_share_cleanup(share_);
    share_ = nullptr;
  }
}

void WebPowerSwitch::initializeRequest() {
  clearRequest();
  request_ = curl_easy_init();
  curl_easy_setopt(request_, CURLOPT_WRITEFUNCTION, writeFunctionStream);
  os_.seekp(0);
  os_.str("");
  curl_easy_setopt(request_, CURLOPT_WRITEDATA, &os_);
	//curl_easy_setopt(request_, CURLOPT_DEBUGFUNCTION, my_trace);
}

void WebPowerSwitch::clearRequest() {
  if (request_ != nullptr) {
    curl_easy_cleanup(request_);
    request_ = nullptr;
  }
}

void WebPowerSwitch::dumpCookies() {
  if (verbose_ == false) {
    return;
  }
	/* extract all known cookies */
	struct curl_slist *cookies = NULL;
	auto res = curl_easy_getinfo(request_, CURLINFO_COOKIELIST, &cookies);
	std::cout << "dumpCookies: res: " << res << " cookies: " << cookies << std::endl;
	if(!res && cookies) {
		/* a linked list of cookies in cookie file format */
		struct curl_slist *each = cookies;
		while(each) {
			//printf("%s\n", each->data);
			std::cout << "    " << each->data << std::endl;
			each = each->next;
		}
		/* we must free these cookies when we are done */
		curl_slist_free_all(cookies);
	}
}

void WebPowerSwitch::prepToFetchOutlets() {
  state_ = STATE_LOGGED_IN;

  name_.clear();
  outlets_.clear();

  initializeRequest();
  curl_easy_setopt(request_, CURLOPT_URL, (prefix_ + host() + "/index.htm").c_str());
  //curl_easy_setopt(request_, CURLOPT_URL, (prefix_ + host() + "/").c_str());
  curl_easy_setopt(request_, CURLOPT_HEADER, 1L);
  curl_easy_setopt(request_, CURLOPT_COOKIEFILE, "");
  curl_easy_setopt(request_, CURLOPT_SHARE, share_);
  curl_easy_setopt(request_, CURLOPT_TIMEOUT, CURL_TIMEOUT);
  if (verboseCurl_) {
    curl_easy_setopt(request_, CURLOPT_VERBOSE, 1L);
  }
  dumpCookies();
  curl_easy_setopt(request_, CURLOPT_FOLLOWLOCATION, 1L);
}

void WebPowerSwitch::buildOutlets(bool force) {
  if (loggedIn_ == false) {
    std::cerr << "not logged in" << std::endl;
    return;
  }

  prepToFetchOutlets();

  auto curlResult = curl_easy_perform(request_);
  if (curlResult != CURLE_OK) {
    return;
  }
  next();
}

void WebPowerSwitch::dumpOutlets(std::ostream& ostr) {
  if (state_ != STATE_OUTLETS_BUILT) {
    ostr << "host: " << host_ << " - no outlets built" << std::endl;
    return;
  }
  ostr << "controller: " << name_ << std::endl;
  ostr << " #  State  Name" << std::endl;
  for (auto outlet : outlets_) {
    ostr << outlet << std::endl;
  }
}

Outlet* WebPowerSwitch::getOutlet(const std::string& name) {
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
  if (loggedIn_ == false) {
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
