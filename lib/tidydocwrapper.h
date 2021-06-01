#ifndef __TIDYDOCWRAPPER_H__INCLUDED__
#define __TIDYDOCWRAPPER_H__INCLUDED__

#include <tidy/tidy.h>

class TidyDocWrapper {
public:
  TidyDocWrapper();
  ~TidyDocWrapper();
  TidyDoc get() const {
    return tdoc_;
  }
  operator TidyDoc() const {
    return tdoc_;
  }

private:
  TidyDoc tdoc_;
};


#endif  /*  __TIDYDOCWRAPPER_H__INCLUDED__  */
