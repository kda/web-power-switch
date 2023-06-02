#include "md5Helper.h"

#include <openssl/evp.h>

namespace md5Helper {

std::vector<unsigned char> calculate(const unsigned char* buf, unsigned int buf_size) {
    EVP_MD_CTX *mdctx;
    unsigned char *md5_digest;
    unsigned int md5_digest_len = EVP_MD_size(EVP_md5());

    // MD5_Init
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

    // MD5_Update
    EVP_DigestUpdate(mdctx, buf, buf_size);

    // MD5_Final
    md5_digest = (unsigned char *)OPENSSL_malloc(md5_digest_len);
    EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);
    std::vector<unsigned char> result(md5_digest, &md5_digest[md5_digest_len]);
    EVP_MD_CTX_free(mdctx);

    return result;
}

}
