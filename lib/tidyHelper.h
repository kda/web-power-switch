#ifndef __TIDYHELPER_H__INCLUDED__
#define __TIDYHELPER_H__INCLUDED__

#include <absl/strings/string_view.h>
#include <tidy/tidy.h>

namespace tidyHelper {

TidyNode findNode(TidyDoc tdoc, absl::string_view name, TidyNode prevNode);
TidyNode findNodeByAttr(TidyDoc tdoc, absl::string_view name,
                        TidyAttrId attrId, absl::string_view attrValue,
                        TidyNode prevNode);
TidyNode findNodeByContent(TidyDoc tdoc, absl::string_view name,
                           absl::string_view content, TidyNode prevNode);
ctmbstr getHeaderValue(TidyDoc tdoc, absl::string_view name);

}

#endif  /*  __TIDYHELPER_H__INCLUDED__  */
