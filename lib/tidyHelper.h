#ifndef __TIDYHELPER_H__INCLUDED__
#define __TIDYHELPER_H__INCLUDED__

#include <string>
#include <tidy/tidy.h>

namespace tidyHelper {

TidyNode findNode(TidyDoc tdoc, const std::string& name, TidyNode prevNode);
TidyNode findNodeByAttr(TidyDoc tdoc, const std::string& name,
                        TidyAttrId attrId, const std::string& attrValue,
                        TidyNode prevNode);
TidyNode findNodeByContent(TidyDoc tdoc, const std::string& name,
                           const std::string& content, TidyNode prevNode);
ctmbstr getHeaderValue(TidyDoc tdoc, const std::string& name);

}

#endif  /*  __TIDYHELPER_H__INCLUDED__  */
