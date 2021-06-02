#include "tidyHelper.h"

#include <iostream>
#include <tidy/tidybuffio.h>

namespace tidyHelper {

TidyNode findNode(TidyNode tnod, const std::string& name, TidyNode& prevNode) {
  if (tnod == nullptr)
    return nullptr;
  //std::cout << "findNode: " << name << std::endl;
  //std::cout << "child: " << tidyGetChild(tnod) << std::endl;
  for (auto child = tidyGetChild(tnod); child; child = tidyGetNext(child)) {
    //std::cout << "child" << std::endl;
    auto nodeName = tidyNodeGetName(child);
    if (nodeName != nullptr) {
      //std::cout << "nodeName: " << nodeName << std::endl;
      if (strncasecmp(nodeName, name.c_str(), strlen(nodeName)) == 0) {
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

TidyNode findNode(TidyDoc tdoc, const std::string& name, TidyNode prevNode) {
  return findNode(tidyGetRoot(tdoc), name, prevNode);
}

TidyNode findNodeByAttr(TidyDoc tdoc, const std::string& name,
                        TidyAttrId attrId, const std::string& attrValue,
                        TidyNode prevNode) {
  auto node = findNode(tdoc, name, prevNode);
  while (node != nullptr) {
    auto value = tidyAttrGetById(node, attrId);
    if (value != nullptr) {
      //std::cout << "nodeAttr: " << tidyAttrValue(value) << std::endl;
      auto valueValue = tidyAttrValue(value);
      if (strncasecmp(valueValue, attrValue.c_str(), strlen(valueValue)) == 0) {
        return node;
      }
    }
    node = findNode(tdoc, name, node);
  }
  return nullptr;
}

TidyNode findNodeByContent(TidyDoc tdoc, const std::string& name,
                           const std::string& content, TidyNode prevNode) {
  auto node = findNode(tdoc, name, prevNode);
  while (node != nullptr) {
    //std::cout << "tidyNodeGetType(node): " << tidyNodeGetType(node) << std::endl;
    auto child = tidyGetChild(node);
    //std::cout << "tidyNodeGetType(child): " << tidyNodeGetType(child) << std::endl;
    if (tidyNodeGetType(child) == TidyNode_Text) {
      TidyBuffer tbuf = {0};
      if (tidyNodeGetText(tdoc, child, &tbuf) == false) {
        std::cout << "tidyNodeGetText failed" << std::endl;
      } else {
        //std::cout << "tbuf.bp: " << tbuf.bp;
        if (content.length() <= tbuf.size &&
            strncmp(content.c_str(), reinterpret_cast<char *>(tbuf.bp), content.length()) == 0) {
          if (prevNode == nullptr) {
            return child;
          }
          if (prevNode == child) {
            prevNode == nullptr;
          }
        }
      }
    }
    node = findNode(tdoc, name, node);
  }
  return nullptr;
}

}
