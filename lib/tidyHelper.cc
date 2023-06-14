#include "tidyHelper.h"

#include <iostream>
#include <tidy/tidybuffio.h>

namespace tidyHelper {

TidyNode findNode(TidyNode tnod, absl::string_view name, TidyNode& prevNode) {
  if (tnod == nullptr)
    return nullptr;
  for (auto child = tidyGetChild(tnod); child; child = tidyGetNext(child)) {
    auto nodeName = tidyNodeGetName(child);
    if (nodeName != nullptr) {
      if (strncasecmp(nodeName, name.data(), strlen(nodeName)) == 0) {
        if (prevNode == nullptr) {
          return child;
        }
        if (prevNode == child) {
          prevNode = nullptr;
        }
      }
    }

    auto foundNode = findNode(child, name, prevNode);
    if (foundNode != nullptr) {
      return foundNode;
    }
  }
  return nullptr;
}

TidyNode findNode(TidyDoc tdoc, absl::string_view name, TidyNode prevNode) {
  return findNode(tidyGetRoot(tdoc), name, prevNode);
}

TidyNode findNodeByAttr(TidyDoc tdoc, absl::string_view name,
                        TidyAttrId attrId, absl::string_view attrValue,
                        TidyNode prevNode) {
  auto node = findNode(tdoc, name, prevNode);
  while (node != nullptr) {
    auto value = tidyAttrGetById(node, attrId);
    if (value != nullptr) {
      auto valueValue = tidyAttrValue(value);
      if (strncasecmp(valueValue, attrValue.data(), strlen(valueValue)) == 0) {
        return node;
      }
    }
    node = findNode(tdoc, name, node);
  }
  return nullptr;
}

TidyNode findNodeByContent(TidyDoc tdoc, absl::string_view name,
                           absl::string_view content, TidyNode prevNode) {
  auto node = findNode(tdoc, name, prevNode);
  while (node != nullptr) {
    auto child = tidyGetChild(node);
    if (tidyNodeGetType(child) == TidyNode_Text) {
      TidyBuffer tbuf = {};
      if (tidyNodeGetText(tdoc, child, &tbuf) == false) {
        std::cout << "tidyNodeGetText failed" << std::endl;
      } else {
        if (content.length() <= tbuf.size &&
            strncmp(content.data(), reinterpret_cast<char *>(tbuf.bp), content.length()) == 0) {
          if (prevNode == nullptr) {
            return child;
          }
          if (prevNode == child) {
            prevNode = nullptr;
          }
        }
      }
    }
    node = findNode(tdoc, name, node);
  }
  return nullptr;
}

}
