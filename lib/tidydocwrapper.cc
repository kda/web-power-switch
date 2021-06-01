#include "tidydocwrapper.h"

TidyDocWrapper::TidyDocWrapper()
: tdoc_(tidyCreate()) {
}

TidyDocWrapper::~TidyDocWrapper() {
  tidyRelease(tdoc_);
}
