#include "auth/RsaVerify.hpp"
#include <memory>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>

namespace chronoplan::auth {

namespace {
template <typename T, void (*Free)(T*)>
struct Deleter { void operator()(T* p) const { if (p) Free(p); } };
template <typename T, void (*Free)(T*)>
using Owned = std::unique_ptr<T, Deleter<T, Free>>;

void freeParamBld(OSSL_PARAM_BLD* p) { OSSL_PARAM_BLD_free(p); }
void freeParams(OSSL_PARAM* p) { OSSL_PARAM_free(p); }
void freeBn(BIGNUM* p) { BN_free(p); }
void freePkey(EVP_PKEY* p) { EVP_PKEY_free(p); }
void freePkeyCtx(EVP_PKEY_CTX* p) { EVP_PKEY_CTX_free(p); }
void freeMdCtx(EVP_MD_CTX* p) { EVP_MD_CTX_free(p); }
}  // namespace

bool rsaSha256Verify(const std::string& modulus, const std::string& exponent,
                     std::string_view message, std::string_view signature) noexcept {
    try {
        if (modulus.size() < 256 || exponent.empty() || exponent.size() > 8 || signature.empty()) return false;

        Owned<BIGNUM, freeBn> n(BN_bin2bn(reinterpret_cast<const unsigned char*>(modulus.data()), static_cast<int>(modulus.size()), nullptr));
        Owned<BIGNUM, freeBn> e(BN_bin2bn(reinterpret_cast<const unsigned char*>(exponent.data()), static_cast<int>(exponent.size()), nullptr));
        if (!n || !e || BN_num_bits(n.get()) < 2048) return false;

        Owned<OSSL_PARAM_BLD, freeParamBld> bld(OSSL_PARAM_BLD_new());
        if (!bld || !OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_RSA_N, n.get()) ||
            !OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_RSA_E, e.get()))
            return false;
        Owned<OSSL_PARAM, freeParams> params(OSSL_PARAM_BLD_to_param(bld.get()));
        Owned<EVP_PKEY_CTX, freePkeyCtx> kctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
        if (!params || !kctx || EVP_PKEY_fromdata_init(kctx.get()) <= 0) return false;

        EVP_PKEY* raw = nullptr;
        if (EVP_PKEY_fromdata(kctx.get(), &raw, EVP_PKEY_PUBLIC_KEY, params.get()) <= 0) return false;
        Owned<EVP_PKEY, freePkey> pkey(raw);

        Owned<EVP_MD_CTX, freeMdCtx> md(EVP_MD_CTX_new());
        if (!md || EVP_DigestVerifyInit(md.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) != 1) return false;   // default padding = PKCS#1 v1.5
        return EVP_DigestVerify(md.get(), reinterpret_cast<const unsigned char*>(signature.data()), signature.size(),
                                reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
    } catch (...) {
        return false;
    }
}

}  // namespace chronoplan::auth
