#ifndef __MD5HELPER_H__INCLUDED__
#define __MD5HELPER_H__INCLUDED__

#include <vector>

namespace md5Helper {

std::vector<unsigned char> calculate(const unsigned char* buf, unsigned int buf_size);

}

#endif  /*  __MD5HELPER_H__INCLUDED__  */
