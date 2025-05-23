/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <evp_utils.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rsa.h>

#include <assert.h>

#define DEFAULT_IV_LEN 12  // For GCM AES and OCB AES the default is 12 (i.e. 96 bits).
#define DEFAULT_KEY_LEN 32
#define DEFAULT_BLOCK_SIZE 128  // For GCM AES, the default block size is 128

/*
 * Description: The EVP_PKEY_new() function allocates an empty EVP_PKEY structure which is used by OpenSSL to store
 * public and private keys. The reference count is set to 1. Return values: EVP_PKEY_new() returns either the newly
 * allocated EVP_PKEY structure or NULL if an error occurred.
 */
EVP_PKEY *EVP_PKEY_new() {
    EVP_PKEY *pkey = malloc(sizeof(EVP_PKEY));

    if (pkey) {
        pkey->references = 1;
        pkey->ec_key     = NULL;
    }

    return pkey;
}

/*
 * Description: EVP_PKEY_get0_EC_KEY() also returns the referenced key in pkey or NULL if the key is not of the correct
 * type but the reference count of the returned key is not incremented and so must not be freed up after use. Return
 * value: EVP_PKEY_get0_EC_KEY() returns the referenced key or NULL if an error occurred.
 */
EC_KEY *EVP_PKEY_get0_EC_KEY(EVP_PKEY *pkey) {
    assert(pkey);

    // In our current model, the key is always of type EC
    return pkey->ec_key;
}

/*
 * Description: EVP_PKEY_set1_EC_KEY() sets the key referenced by pkey to key.
 * Return values: EVP_PKEY_set1_EC_KEY() returns 1 for success or 0 for failure.
 */
int EVP_PKEY_set1_EC_KEY(EVP_PKEY *pkey, EC_KEY *key) {
    if (pkey == NULL || key == NULL || nondet_bool()) {
        return 0;
    }

    EC_KEY_up_ref(key);
    pkey->ec_key = key;

    return 1;
}

/*
 * Description: EVP_PKEY_free() decrements the reference count of key and, if the reference count is zero, frees it up.
 * If key is NULL, nothing is done.
 */
void EVP_PKEY_free(EVP_PKEY *pkey) {
    if (pkey != NULL &&
        /* We must include this extra guard to avoid spurious arithmetic underflows. */
        pkey->references > 0) {
        pkey->references -= 1;
        if (pkey->references == 0) {
            EC_KEY_free(pkey->ec_key);
            free(pkey);
        }
    }
}

bool evp_pkey_is_valid(EVP_PKEY *pkey);

/*
 * Description: The EVP_PKEY_CTX_new() function allocates public key algorithm context using the algorithm specified in
 * pkey and ENGINE e. Return values: EVP_PKEY_CTX_new() returns either the newly allocated EVP_PKEY_CTX structure of
 * NULL if an error occurred.
 */
EVP_PKEY_CTX *EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e) {
    assert(evp_pkey_is_valid(pkey));
    assert(!e);  // Assuming is always called with e == NULL

    EVP_PKEY_CTX *ctx = malloc(sizeof(EVP_PKEY_CTX));

    if (ctx) {
        ctx->is_initialized_for_signing    = false;
        ctx->is_initialized_for_derivation = false;
        ctx->is_initialized_for_encryption = false;
        ctx->is_initialized_for_decryption = false;
        ctx->pkey                          = pkey;
        pkey->references += 1;
    }

    return ctx;
}

/*
 * Description: The EVP_PKEY_CTX_new_id() function allocates public key algorithm
 * context using the algorithm specified by id and ENGINE e. It is normally used when no EVP_PKEY structure is
 * associated with the operations, for example during parameter generation of key generation for some algorithms. Return
 * values: EVP_PKEY_CTX_new_id() returns either the newly allocated EVP_PKEY_CTX structure of NULL if an error occurred.
 */
EVP_PKEY_CTX *EVP_PKEY_CTX_new_id(int id, ENGINE *e) {
    // assert(!e);  // Assuming is always called with e == NULL

    EVP_PKEY_CTX *ctx = malloc(sizeof(EVP_PKEY_CTX));

    if (ctx) {
        ctx->is_initialized_for_signing    = false;
        ctx->is_initialized_for_derivation = false;
        ctx->is_initialized_for_encryption = false;
        ctx->is_initialized_for_decryption = false;
        ctx->pkey                          = NULL;
    }

    return ctx;
}

/* Description: The EVP_PKEY_derive_init() function initializes a public key algorithm context using key pkey
 * for shared secret derivation. EVP_PKEY_derive_init() returns 1 for success and 0 or a negative
 * value for failure. In particular a return value of -2 indicates the operation is not supported by the public key
 * algorithm.
 */
int EVP_PKEY_derive_init(EVP_PKEY_CTX *ctx) {
    assert(ctx);
    if (nondet_bool()) {
        ctx->is_initialized_for_derivation = true;
        return 1;
    }

    int rv;
    __CPROVER_assume(rv <= 0);
    return rv;
}

/*
 * Description: The EVP_PKEY_sign_init() function initializes a public key algorithm context using key pkey for a
 * signing operation. Return values: EVP_PKEY_sign_init() and EVP_PKEY_sign() return 1 for success and 0 or a negative
 * value for failure. In particular a return value of -2 indicates the operation is not supported by the public key
 * algorithm.
 */
int EVP_PKEY_sign_init(EVP_PKEY_CTX *ctx) {
    assert(ctx);
    assert(ctx->pkey);

    if (nondet_bool()) {
        ctx->is_initialized_for_signing = true;
        return 1;
    }

    int rv;
    __CPROVER_assume(rv <= 0);
    return rv;
}

bool evp_pkey_ctx_is_valid(EVP_PKEY_CTX *);

/*
 * Description: The EVP_PKEY_sign() function performs a public key signing operation using ctx. The data to be signed is
 * specified using the tbs and tbslen parameters. If sig is NULL then the maximum size of the output buffer is written
 * to the siglen parameter. If sig is not NULL then before the call the siglen parameter should contain the length of
 * the sig buffer, if the call is successful the signature is written to sig and the amount of data written to siglen.
 * Return values: EVP_PKEY_sign_init() and EVP_PKEY_sign() return 1 for success and 0 or a negative value for failure.
 * In particular a return value of -2 indicates the operation is not supported by the public key algorithm.
 */
int EVP_PKEY_sign(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen, const unsigned char *tbs, size_t tbslen) {
    assert(evp_pkey_ctx_is_valid(ctx));
    assert(ctx->is_initialized_for_signing == true);
    assert(siglen);
    assert(!sig || (*siglen >= max_signature_size() && __CPROVER_w_ok(sig, *siglen)));
    assert(tbs);
    assert(__CPROVER_r_ok(tbs, tbslen));

    if (nondet_bool()) {
        int rv;
        __CPROVER_assume(rv <= 0);
        return rv;
    }

    // Signature size is nondeterministic but fixed. See ec_override.c for details.
    size_t max_required_size = max_signature_size();

    if (!sig) {
        *siglen = max_required_size;
    } else {
        size_t amount_of_data_written;
        __CPROVER_assume(amount_of_data_written <= max_required_size);
        write_unconstrained_data(sig, amount_of_data_written);
        *siglen = amount_of_data_written;
    }

    return 1;
}

/*
 * Description: The function EVP_PKEY_CTX_ctrl() sends a control operation to the context ctx.
 * The key type used must match keytype if it is not -1. The parameter optype is a mask indicating which operations
 * the control can be applied to. The control command is indicated in cmd and any additional arguments in p1 and p2.
 * EVP_PKEY_CTX_ctrl() and its macros return a positive value for success and 0 or a negative value for failure.
 * In particular a return value of -2 indicates the operation is not supported by the public key algorithm.
 */
int EVP_PKEY_CTX_ctrl(EVP_PKEY_CTX *ctx, int keytype, int optype, int cmd, int p1, void *p2) {
    assert(ctx != NULL);
    assert(keytype == -1);  // Is this ever false?
    if (nondet_bool()) {
        return 1;
    }
    int rv;
    __CPROVER_assume(rv <= 0);
    return rv;
}

/*
 * Description: The EVP_PKEY_derive() derives a shared secret using ctx. If key is NULL then the maximum size of the
 * output buffer is written to the keylen parameter. If key is not NULL then before the call the keylen parameter should
 * contain the length of the key buffer, if the call is successful the shared secret is written to key and the amount of
 * data written to keylen.
 */
int EVP_PKEY_derive(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen) {
    /* TODO: assert(evp_pkey_ctx_is_valid(ctx)); */
    assert(ctx != NULL);
    assert(ctx->is_initialized_for_derivation == true);
    assert(keylen);
    // Derivation size is nondeterministic but fixed. See ec_override.c for details.
    size_t max_required_size = max_derivation_size();

    if (nondet_bool()) {
        int rv;
        __CPROVER_assume(rv <= 0);
        return rv;
    }

    if (!key) {
        *keylen = max_required_size;
    } else {
        size_t amount_of_data_written;
        __CPROVER_assume(amount_of_data_written <= *keylen);
        write_unconstrained_data(key, amount_of_data_written);
        *keylen = amount_of_data_written;
    }

    return 1;
}

/*
 * The EVP_PKEY_encrypt_init() function initializes a public key algorithm context using key pkey for an encryption
 * operation. EVP_PKEY_encrypt_init() and EVP_PKEY_encrypt() return 1 for success and 0 or a negative value for failure.
 * In particular a return value of -2 indicates the operation is not supported by the public key algorithm.
 */
int EVP_PKEY_encrypt_init(EVP_PKEY_CTX *ctx) {
    assert(ctx != NULL);
    assert(ctx->pkey != NULL);
    if (nondet_bool()) {
        ctx->is_initialized_for_encryption = true;
        return 1;
    }
    int rv;
    __CPROVER_assume(rv <= 0);
    return rv;
}

/*
 * The EVP_PKEY_decrypt_init() function initializes a public key algorithm context using key pkey for a decryption
 * operation. EVP_PKEY_decrypt_init() and EVP_PKEY_decrypt() return 1 for success and 0 or a negative value for failure.
 * In particular a return value of -2 indicates the operation is not supported by the public key algorithm.
 */
int EVP_PKEY_decrypt_init(EVP_PKEY_CTX *ctx) {
    assert(ctx != NULL);
    assert(ctx->pkey != NULL);
    if (nondet_bool()) {
        ctx->is_initialized_for_decryption = true;
        return 1;
    }
    int rv;
    __CPROVER_assume(rv <= 0);
    return rv;
}

/*
 * The macro EVP_PKEY_CTX_set_rsa_padding() sets the RSA padding mode for ctx. The pad parameter can take the value
 * RSA_PKCS1_PADDING for PKCS#1 padding, RSA_SSLV23_PADDING for SSLv23 padding, RSA_NO_PADDING for no padding,
 * RSA_PKCS1_OAEP_PADDING for OAEP padding (encrypt and decrypt only), RSA_X931_PADDING for X9.31 padding (signature
 * operations only) and RSA_PKCS1_PSS_PADDING (sign and verify only).
 *
 */
int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX *ctx, int pad) {
    assert(ctx != NULL);
    assert(
        pad == RSA_PKCS1_PADDING || pad == RSA_SSLV23_PADDING || pad == RSA_NO_PADDING ||
        pad == RSA_PKCS1_OAEP_PADDING || pad == RSA_X931_PADDING || pad == RSA_PKCS1_PSS_PADDING);
    assert(IMPLIES(pad == RSA_X931_PADDING, ctx->is_initialized_for_signing));
    ctx->rsa_pad = pad;
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}
/*
 * The EVP_PKEY_CTX_set_rsa_oaep_md() macro sets the message digest type used in RSA OAEP to md.
 * The padding mode must have been set to RSA_PKCS1_OAEP_PADDING.
 */
int EVP_PKEY_CTX_set_rsa_oaep_md(EVP_PKEY_CTX *ctx, const EVP_MD *md) {
    assert(ctx != NULL);
    assert(ctx->rsa_pad == RSA_PKCS1_OAEP_PADDING);
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * The EVP_PKEY_CTX_set_rsa_mgf1_md() macro sets the MGF1 digest for RSA padding schemes to md.
 * If not explicitly set the signing digest is used. The padding mode must have been set to RSA_PKCS1_OAEP_PADDING or
 * RSA_PKCS1_PSS_PADDING.
 */
int EVP_PKEY_CTX_set_rsa_mgf1_md(EVP_PKEY_CTX *ctx, const EVP_MD *md) {
    assert(ctx != NULL);
    assert(ctx->rsa_pad == RSA_PKCS1_OAEP_PADDING || ctx->rsa_pad == RSA_PKCS1_PSS_PADDING);
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * The EVP_PKEY_encrypt() function performs a public key encryption operation using ctx.
 * The data to be encrypted is specified using the in and inlen parameters. If out is NULL then the maximum size of the
 * output buffer is written to the outlen parameter. If out is not NULL then before the call the outlen parameter should
 * contain the length of the out buffer, if the call is successful the encrypted data is written to out and the amount
 * of data written to outlen.
 */
int EVP_PKEY_encrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char *in, size_t inlen) {
    assert(ctx != NULL);
    // Encyption size is nondeterministic but fixed. See ec_override.c for details.
    size_t max_required_size = max_encryption_size();

    if (nondet_bool()) {
        int rv;
        __CPROVER_assume(rv <= 0);
        return rv;
    }

    if (!out) {
        *outlen = max_required_size;
    } else {
        size_t amount_of_data_written;
        __CPROVER_assume(amount_of_data_written <= *outlen);
        write_unconstrained_data(out, amount_of_data_written);
        *outlen = amount_of_data_written;
    }

    return 1;
}

/*
 * The EVP_PKEY_decrypt() function performs a public key decryption operation using ctx. The data to be decrypted is
 * specified using the in and inlen parameters. If out is NULL then the maximum size of the output buffer is written to
 * the outlen parameter. If out is not NULL then before the call the outlen parameter should contain the length of the
 * out buffer, if the call is successful the decrypted data is written to out and the amount of data written to outlen.
 */
int EVP_PKEY_decrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char *in, size_t inlen) {
    assert(ctx != NULL);
    // Decryption size is nondeterministic but fixed. See ec_override.c for details.
    size_t max_required_size = max_decryption_size();

    if (nondet_bool()) {
        int rv;
        __CPROVER_assume(rv <= 0);
        return rv;
    }

    if (!out) {
        *outlen = max_required_size;
    } else {
        size_t amount_of_data_written;
        __CPROVER_assume(amount_of_data_written <= *outlen);
        write_unconstrained_data(out, amount_of_data_written);
        *outlen = amount_of_data_written;
    }

    return 1;
}

/*
 * EVP_PKEY_CTX_free() frees up the context ctx. If ctx is NULL, nothing is done.
 *
 * Reference Implementation:
 * https://github.com/openssl/openssl/blob/6c9bc258d2e9e7b500236a1c696da1f384f0b907/crypto/evp/pmeth_lib.c#L393
 */
void EVP_PKEY_CTX_free(EVP_PKEY_CTX *ctx) {
    if (!ctx) return;

    EVP_PKEY_free(ctx->pkey);
    free(ctx);
}

/*
 * Description: AES for 128, 192 and 256 bit keys in Galois Counter Mode (GCM). These ciphers require additional control
 * operations to function correctly, see the "AEAD Interface" in EVP_EncryptInit(3) section for details. Return values:
 * These functions return an EVP_CIPHER structure that contains the implementation of the symmetric cipher.
 */
const EVP_CIPHER *EVP_aes_128_gcm(void) {
    static const EVP_CIPHER cipher = { EVP_AES_128_GCM, 128 };
    return &cipher;
}
const EVP_CIPHER *EVP_aes_192_gcm(void) {
    static const EVP_CIPHER cipher = { EVP_AES_192_GCM, 128 };
    return &cipher;
}
const EVP_CIPHER *EVP_aes_256_gcm(void) {
    static const EVP_CIPHER cipher = { EVP_AES_256_GCM, 128 };
    return &cipher;
}
const EVP_CIPHER *EVP_aes_128_ecb(void) {
    static const EVP_CIPHER cipher = { EVP_AES_128_ECB, 128 };
    return &cipher;
}

/* From MAN pages: EVP_CIPHER_CTX_init() initializes cipher contex
 * ctx. */
/* It's not entirely clear what this function is intended to do. */
void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *ctx) {
    return;
}

/*
 * EVP_CIPHER_CTX_new() creates a cipher context.
 */
EVP_CIPHER_CTX *EVP_CIPHER_CTX_new() {
    EVP_CIPHER_CTX *cipher_ctx = malloc(sizeof(EVP_CIPHER_CTX));
    if (cipher_ctx) {
        cipher_ctx->iv_len         = DEFAULT_IV_LEN;
        cipher_ctx->iv_set         = false;
        cipher_ctx->key_len        = DEFAULT_KEY_LEN;
        cipher_ctx->padding        = true;
        cipher_ctx->data_processed = false;
        cipher_ctx->data_remaining = 0;
        cipher_ctx->cipher         = NULL;
    }
    return cipher_ctx;
}

/*
 * EVP_CipherInit_ex(), EVP_CipherUpdate() and EVP_CipherFinal_ex() are functions that can be used for
 * decryption or encryption. The operation performed depends on the value of the enc parameter.
 * It should be set to 1 for encryption, 0 for decryption and -1 to leave the value unchanged (the actual value of 'enc'
 * being supplied in a previous call). Return 1 for success and 0 for failure.
 */
int EVP_CipherInit_ex(
    EVP_CIPHER_CTX *ctx,
    const EVP_CIPHER *cipher,
    ENGINE *impl,
    const unsigned char *key,
    const unsigned char *iv,
    int enc) {
    assert(ctx != NULL);
    assert(enc == 0 || enc == 1 || enc == -1);
    if (enc != -1) {
        ctx->encrypt = enc;
    }
    if (cipher) {
        ctx->cipher = cipher;
    }
    if (iv) {
        ctx->iv_set = true;
    }
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * EVP_CIPHER_CTX_ctrl() allows various cipher specific parameters to be determined and set.
 */
int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr) {
    if (type == EVP_CTRL_GCM_SET_IVLEN || type == EVP_CTRL_AEAD_SET_IVLEN) {
        assert(ctx->iv_set == false);
        /* iv length must be positive */
        assert(arg > 0);
        ctx->iv_len = arg;
    }

    /* Only legal when encrypting data. */
    assert(IMPLIES(type == EVP_CTRL_GCM_GET_TAG, ctx->encrypt == 1));
    assert(IMPLIES(type == EVP_CTRL_GCM_GET_TAG, ctx->data_processed == true));
    /* Need to be able to write taglen (arg) bytes to buffer ptr. */
    assert(IMPLIES(type == EVP_CTRL_GCM_GET_TAG, __CPROVER_w_ok(ptr, arg)));

    /* Only legal when decrypting data. */
    assert(IMPLIES(type == EVP_CTRL_GCM_SET_TAG, ctx->encrypt == 0));
    /* Need to be able to write taglen (arg) bytes to buffer ptr. */
    assert(IMPLIES(type == EVP_CTRL_GCM_SET_TAG, __CPROVER_w_ok(ptr, arg)));

    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * EVP_CIPHER_CTX_free() clears all information from a cipher context and free up any allocated memory associate with
 * it, including ctx itself. This function should be called after all operations using a cipher are complete so
 * sensitive information does not remain in memory.
 */
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx) {
    if (ctx) {
        free(ctx);
    }
}

/*
 * EVP_EncryptInit_ex() sets up cipher context ctx for encryption with cipher type from ENGINE impl.
 * ctx must be created before calling this function. type is normally supplied by a function such as EVP_aes_256_cbc().
 * If impl is NULL then the default implementation is used. key is the symmetric key to use and iv is the IV to use (if
 * necessary), the actual number of bytes used for the key and IV depends on the cipher. It is possible to set all
 * parameters to NULL except type in an initial call and supply the remaining parameters in subsequent calls, all of
 * which have type set to NULL. This is done when the default cipher parameters are not appropriate.
 */
int EVP_EncryptInit_ex(
    EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type, ENGINE *impl, const unsigned char *key, const unsigned char *iv) {
    assert(ctx != NULL);
    ctx->encrypt = 1;
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * EVP_DecryptInit_ex() is the corresponding decryption operation.
 */
int EVP_DecryptInit_ex(
    EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type, ENGINE *impl, const unsigned char *key, const unsigned char *iv) {
    assert(ctx != NULL);
    assert(type != NULL);
    ctx->encrypt = 0;
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * EVP_CipherInit_ex(), EVP_CipherUpdate() and EVP_CipherFinal_ex() are functions that can be used for decryption
 * or encryption. The operation performed depends on the value of the enc parameter. It should be set to 1 for
 * encryption, 0 for decryption and -1 to leave the value unchanged (the actual value of 'enc' being supplied in a
 * previous call). Return 1 for success and 0 for failure.
 * To specify any additional authenticated data (AAD) a call to EVP_CipherUpdate(), EVP_EncryptUpdate() or
 * EVP_DecryptUpdate() should be made with the output parameter out set to NULL.
 */
int EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl) {
    assert(ctx != NULL);
    if (ctx->encrypt) {
        return EVP_EncryptUpdate(ctx, out, outl, in, inl);
    } else
        return EVP_DecryptUpdate(ctx, out, outl, in, inl);
}

/*
 * EVP_EncryptUpdate() encrypts inl bytes from the buffer in and writes the encrypted version to out.
 * This function can be called multiple times to encrypt successive blocks of data. The amount of data written depends
 * on the block alignment of the encrypted data: as a result the amount of data written may be anything from zero bytes
 * to (inl + cipher_block_size - 1) so out should contain sufficient room. The actual number of bytes written is placed
 * in outl. It also checks if in and out are partially overlapping, and if they are 0 is returned to indicate failure.
 */
int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl) {
    assert(ctx != NULL);
    assert(ctx->data_processed == false);
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    if (out == NULL) {  // specifying aad
        return rv;
    }
    size_t out_size;
    __CPROVER_assume(out_size >= 0);
    if (ctx->cipher) {
        __CPROVER_assume(out_size <= inl - 1);
    } else {
        __CPROVER_assume(out_size <= inl);
        ctx->data_remaining = inl - out_size;
    }
    /*
     * This check is redundant with the following __CPROVER_w_ok.
     * __CPROVER_w_ok is a macro for __CPROVER_w_ok primitive, which
     * should return true if out is writable upt to out_size bytes;
     * however, __CPROVER_w_ok has been replaced by a simple nullness check for now.
     * Thus, we also include an additional check using __CPROVER_OBJECT_SIZE.
     */
    assert(__CPROVER_OBJECT_SIZE(out) >= out_size);
    assert(__CPROVER_w_ok(out, out_size));
    *outl = out_size;
    return rv;
}

/*
 * EVP_DecryptUpdate() is the corresponding decryption operation.
 * EVP_DecryptFinal() will return an error code if padding is enabled and the final block is not correctly formatted.
 * The parameters and restrictions are identical to the encryption operations except that if padding is enabled the
 * decrypted data buffer out passed to EVP_DecryptUpdate() should have sufficient room for (inl + cipher_block_size)
 * bytes unless the cipher block size is 1 in which case inl bytes is sufficient.
 */
int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl) {
    assert(ctx != NULL);
    assert(ctx->data_processed == false);
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    if (out == NULL) {  // specifying aad
        return rv;
    }
    size_t out_size;
    __CPROVER_assume(out_size >= 0);
    if (ctx->cipher) {
        if (ctx->padding) {
            __CPROVER_assume(out_size <= inl);
        }
    } else {
        __CPROVER_assume(out_size <= inl);
        ctx->data_remaining = inl - out_size;
    }
    /*
     * This check is redundant with the following __CPROVER_w_ok.
     * __CPROVER_w_ok is a macro for __CPROVER_w_ok primitive, which
     * should return true if out is writable upt to out_size bytes;
     * however, __CPROVER_w_ok has been replaced by a simple nullness check for now.
     * Thus, we also include an additional check using __CPROVER_OBJECT_SIZE.
     */
    assert(__CPROVER_OBJECT_SIZE(out) >= out_size);
    assert(__CPROVER_w_ok(out, out_size));
    *outl = out_size;
    return rv;
}

/*
 * If padding is enabled (the default) then EVP_EncryptFinal_ex() encrypts the "final" data, that is any data that
 * remains in a partial block. It uses standard block padding (aka PKCS padding).
 * The encrypted final data is written to out which should have sufficient space for one cipher block.
 * The number of bytes written is placed in outl. After this function is called the encryption operation is finished and
 * no further calls to EVP_EncryptUpdate() should be made.
 */
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl) {
    assert(ctx != NULL);
    if (ctx->padding == true) {
        *outl = ctx->data_remaining;
        assert(__CPROVER_w_ok(out, ctx->data_remaining));
    }
    ctx->data_processed = true;
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}
/*
 * EVP_DecryptFinal_ex() is the corresponding decryption operation.
 * EVP_DecryptFinal() will return an error code if padding is enabled and the final block is not correctly formatted.
 * The parameters and restrictions are identical to the encryption operations except that if padding is enabled the
 * decrypted data buffer out passed to EVP_DecryptUpdate() should have sufficient room for (inl + cipher_block_size)
 * bytes unless the cipher block size is 1 in which case inl bytes is sufficient.
 */
int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl) {
    assert(ctx != NULL);
    if (ctx->padding == true) {
        *outl = ctx->data_remaining;
        assert(__CPROVER_w_ok(outm, ctx->data_remaining));
    }
    ctx->data_processed = true;
    int rv;
    __CPROVER_assume(rv == 0 || rv == 1);
    return rv;
}

/*
 * Description: The SHA-2 SHA-224, SHA-256, SHA-512/224, SHA512/256, SHA-384 and SHA-512 algorithms, which generate 224,
 * 256, 224, 256, 384 and 512 bits respectively of output from a given input. Return values: These functions return a
 * EVP_MD structure that contains the implementation of the symmetric cipher.
 */
const EVP_MD *EVP_md5() {
    static const EVP_MD md = { EVP_MD5, 0, 0, 16 /* Digest length. */, 0, 0, 16 };
    return &md;
}
const EVP_MD *EVP_sha1() {
    static const EVP_MD md = { EVP_SHA1, 0, 0, 20 /* Digest length. */, 0, 0, 20 };
    return &md;
}
const EVP_MD *EVP_sha224() {
    static const EVP_MD md = { EVP_SHA224, 0, 0, 28 /* Digest length. */, 0, 0, 28 };
    return &md;
}
const EVP_MD *EVP_sha256() {
    static const EVP_MD md = { EVP_SHA256, 0, 0, 32 /* Digest length. */, 0, 0, 32 };
    return &md;
}
const EVP_MD *EVP_sha384() {
    static const EVP_MD md = { EVP_SHA384, 0, 0, 48 /* Digest length. */, 0, 0, 48 };
    return &md;
}
const EVP_MD *EVP_sha512() {
    static const EVP_MD md = { EVP_SHA512, 0, 0, 64 /* Digest length. */, 0, 0, 64 };
    return &md;
}

/* Description: Return the size of the message digest when passed an EVP_MD or an EVP_MD_CTX structure, i.e. the size of
 * the hash.
 */
int EVP_MD_size(const EVP_MD *md) {
    assert(md != NULL);
    if (md->from == EVP_MD5) {
        return 16;
    }
    if (md->from == EVP_SHA1) {
        return 20;
    }
    if (md->from == EVP_SHA224) {
        return 28;
    }
    if (md->from == EVP_SHA256) {
        return 32;
    }
    if (md->from == EVP_SHA384) {
        return 48;
    }
    return 64;
}

/* Helper function for CBMC proofs: checks if EVP_MD_CTX is valid. */
bool evp_md_ctx_is_valid(EVP_MD_CTX *ctx) {
    return ctx && ctx->digest != NULL && ctx->digest->md_size <= EVP_MAX_MD_SIZE &&
           (ctx->pctx == NULL || evp_pkey_ctx_is_valid(ctx->pctx));
}

/*
 * Description: Allocates and returns a digest context.
 */
EVP_MD_CTX *EVP_MD_CTX_new() {
    EVP_MD_CTX *ctx = malloc(sizeof(*ctx));

    if (ctx != NULL) {
        ctx->digest  = NULL;
        ctx->md_data = NULL;
        ctx->pctx    = NULL;
    }

    return ctx;
}

/*
 * Description: Return the size of the message digest when passed an EVP_MD or an EVP_MD_CTX structure, i.e. the size of
 * the hash. Return values: Returns the digest or block size in bytes.
 */
int EVP_MD_CTX_size(const EVP_MD_CTX *ctx) {
    assert(ctx != NULL);
    return EVP_MD_size(ctx->digest);
}

/*
 * Description: Cleans up digest context ctx and frees up the space allocated to it.
 */
void EVP_MD_CTX_free(EVP_MD_CTX *ctx) {
    if (ctx != NULL) {
        free(ctx->digest);
        free(ctx->md_data);
        EVP_PKEY_CTX_free(ctx->pctx);
        free(ctx);
    }
}

/*
 * Description: This call frees resources associated with the context.
 */
int EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx) {
    if (nondet_bool()) return 0;
    if (ctx != NULL) {
        free(ctx->digest);
        free(ctx->md_data);
        EVP_PKEY_CTX_free(ctx->pctx);
    }
    return 1;
}

/*
 * Description: Returns the EVP_MD structure corresponding to the passed EVP_MD_CTX.
 */
const EVP_MD *EVP_MD_CTX_md(const EVP_MD_CTX *ctx) {
    if (ctx == NULL) return NULL;
    return ctx->digest;
}

bool evp_md_is_valid(EVP_MD *md);

/*
 * Description: Sets up digest context ctx to use a digest type from ENGINE impl. type will typically be supplied by
 * a function such as EVP_sha1(). If impl is NULL then the default implementation of digest type is used. Return
 * values: Returns 1 for success and 0 for failure.
 */
int EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl) {
    assert(ctx != NULL);  // ctx must be initialized before calling EVP_DigestInit_ex function.
    assert(evp_md_is_valid(type));
    assert(impl == NULL);  // Assuming that this function is always called with impl == NULL

    if (nondet_bool()) return 0;

    ctx->digest  = type;
    ctx->md_data = malloc(type->md_size);
    ctx->pctx    = NULL;

    return 1;
}

/*
 * Description: Behaves in the same way as EVP_DigestInit_ex() except it always uses the default digest implementation.
 * Return value: Returns 1 for success and 0 for failure.
 */
int EVP_DigestInit(EVP_MD_CTX *ctx, const EVP_MD *type) {
    return EVP_DigestInit_ex(ctx, type, NULL);
}

/*
 * Description: Hashes cnt bytes of data at d into the digest context ctx. This function can be called several times
 * on the same ctx to hash additional data. Return values: Returns 1 for success and 0 for failure.
 */
int EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d, size_t cnt) {
    assert(ctx != NULL);
    assert(ctx->digest != NULL);
    assert(cnt == 0 || __CPROVER_r_ok(d, cnt));

    __CPROVER_havoc_object(ctx->digest);
    if (nondet_bool()) {
        return 0;
    }
    return 1;
}

/*
 * Description: Retrieves the digest value from ctx and places it in md. If the s parameter is not NULL then the
 * number of bytes of data written (i.e. the length of the digest) will be written to the integer at s, at most
 * EVP_MAX_MD_SIZE bytes will be written. After calling EVP_DigestFinal_ex() no additional calls to
 * EVP_DigestUpdate() can be made, but EVP_DigestInit_ex() can be called to initialize a new digest operation.
 * Return values: Returns 1 for success and 0 for failure.
 */
int EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *s) {
    assert(ctx != NULL);
    assert(__CPROVER_w_ok(md, EVP_MD_CTX_size(ctx)));
    // s can be NULL

    *md = nondet_unsigned_char();

    if (s) *s = EVP_MD_CTX_size(ctx);
    ctx->digest = NULL; /* No additional calls to EVP_DigestUpdate. */

    if (nondet_bool()) {
        // Something went wrong, can't guarantee *s will have the correct value
        unsigned int garbage;
        if (s) *s = garbage;
        return 0;
    }

    return 1;
}

/*
 * Description: Similar to EVP_DigestFinal_ex() except the digest context ctx is automatically cleaned up.
 * Return values: Returns 1 for success and 0 for failure.
 */
int EVP_DigestFinal(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *s) {
    int ret;
    ret = EVP_DigestFinal_ex(ctx, md, s);
    // Context is "cleaned up", but not sure how this restricts future operations
    // Assuming that EVP_PKEY is not freed, and that EVP_MD_CTX_free still needs to be called
    return ret;
}

/*
 * Description: EVP_DigestVerifyInit() sets up verification context ctx to use digest type from ENGINE e and public key
 * pkey. ctx must be created with EVP_MD_CTX_new() before calling this function. If pctx is not NULL, the EVP_PKEY_CTX
 * of the verification operation will be written to *pctx: this can be used to set alternative verification options.
 * Note that any existing value in *pctx is overwritten. The EVP_PKEY_CTX value returned must not be freed directly by
 * the application if ctx is not assigned an EVP_PKEY_CTX value before being passed to EVP_DigestVerifyInit() (which
 * means the EVP_PKEY_CTX is created inside EVP_DigestVerifyInit() and it will be freed automatically when the
 * EVP_MD_CTX is freed).
 * Return values: EVP_DigestVerifyInit() EVP_DigestVerifyUpdate() return 1 for success and 0 for
 * failure.
 */
int EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx, const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey) {
    assert(ctx != NULL);
    assert(!pctx);  // Assuming that this function is always called in ESDK with pctx == NULL
    assert(evp_md_is_valid(type));
    assert(!e);  // Assuming that this function is always called in ESDK with e == NULL
    assert(evp_pkey_is_valid(pkey));

    if (nondet_bool()) return 0;

    /*ctx->is_initialized = true;
    ctx->pkey           = pkey;
    pkey->references += 1;
    ctx->digest_size = type->size;*/

    return 1;
}

/*
 * Description: EVP_DigestVerifyFinal() verifies the data in ctx against the signature in sig of length siglen.
 * Return values: EVP_DigestVerifyFinal() and EVP_DigestVerify() return 1 for success; any other value indicates
 * failure. A return value of zero indicates that the signature did not verify successfully (that is, tbs did not match
 * the original data or the signature had an invalid form), while other values indicate a more serious error (and
 * sometimes also indicate an invalid signature form).
 */
int EVP_DigestVerifyFinal(EVP_MD_CTX *ctx, const unsigned char *sig, size_t siglen) {
    assert(evp_md_ctx_is_valid(ctx));
    assert(sig);
    assert(__CPROVER_r_ok(sig, siglen));

    // Since this operation only performs verification, none of the arguments are modified

    return nondet_int();
}

/* Abstraction of the HMAC_CTX struct has been moved to hmcac.h*/

/*
 * Description: HMAC_CTX_init() initialises a HMAC_CTX before first use. It must be called.
 */
void HMAC_CTX_init(HMAC_CTX *ctx) {
    HMAC_CTX *ctx_new = malloc(sizeof(HMAC_CTX));
    __CPROVER_assume(ctx_new);  // cannot be null
    ctx_new->is_initialized = true;
    ctx_new->md             = malloc(sizeof(EVP_MD));
    *ctx                    = *ctx_new;
}

/*
HMAC() computes the message authentication code of the n bytes at d using the hash function evp_md and the
key key which is key_len bytes long.
It places the result in md (which must have space for the output of the hash function,
which is no more than EVP_MAX_MD_SIZE bytes). If md is NULL, the digest is placed in a static array.
The size of the output is placed in md_len, unless it is NULL.
Note: passing a NULL value for md to use the static array is not thread safe.
*/

unsigned char *HMAC(
    const EVP_MD *evp_md,
    const void *key,
    int key_len,
    const unsigned char *d,
    size_t n,
    unsigned char *md,
    unsigned int *md_len) {
    assert(evp_md != NULL);
    size_t amount_of_data_written;
    __CPROVER_assume(amount_of_data_written <= EVP_MAX_MD_SIZE);
    if (md != NULL) {
        write_unconstrained_data(md, amount_of_data_written);
        *md_len = amount_of_data_written;
        return md;
    }
    // create a static array to return the result
    unsigned char *res = malloc(sizeof(unsigned char) * (amount_of_data_written + 1));
    write_unconstrained_data(res, amount_of_data_written);
    return res;
}

bool hmac_ctx_is_valid(HMAC_CTX *ctx);

/*
* HMAC_Init_ex() initializes or reuses a HMAC_CTX structure to use the hash function evp_md and key key.
* If both are NULL (or evp_md is the same as the previous digest used by ctx and key is NULL) the existing key is
reused.
* ctx must have been created with HMAC_CTX_new() before the first use of an HMAC_CTX in this function.
* N.B. HMAC_Init() had this undocumented behaviour in previous versions of OpenSSL - failure to switch to HMAC_Init_ex()
in
* programs that expect it will cause them to stop working.

* NB: if HMAC_Init_ex() is called with key NULL and evp_md is not the same as the previous digest used by ctx then an
* error is returned because reuse of an existing key with a different digest is not supported.
*
* Return 1 for success or 0 if an error occurred.
*/
int HMAC_Init_ex(HMAC_CTX *ctx, const void *key, int len, const EVP_MD *md, ENGINE *impl) {
    assert(hmac_ctx_is_valid(ctx));
    if (md != NULL) {
        if (key != NULL) {
            ctx->md = md;
        }
    }
    int rv;
    __CPROVER_assume(rv == 1 || rv == 0);
    return rv;
}

/*
 * HMAC_Update() can be called repeatedly with chunks of the message to be authenticated (len bytes at data).
 * Return 1 for success or 0 if an error occurred.
 */
int HMAC_Update(HMAC_CTX *ctx, const unsigned char *data, size_t len) {
    assert(hmac_ctx_is_valid(ctx));
    int rv;
    __CPROVER_assume(rv == 1 || rv == 0);
    return rv;
}

/*
 *HMAC_Final() places the message authentication code in md, which must have space for the hash function output.
 */
int HMAC_Final(HMAC_CTX *ctx, unsigned char *md, unsigned int *len) {
    assert(hmac_ctx_is_valid(ctx));
    assert(ctx->md != NULL);
    int md_size = EVP_MD_size(ctx->md);
    __CPROVER_w_ok(md, md_size);
    *len = md_size;
    int rv;
    __CPROVER_assume(rv == 1 || rv == 0);
    /*
     * Using __CPROVER_r_ok(md, md_size)  would make this assumption stronger,
     * but the use of these primitives in assumptions may lead to spurious results.
     */
    __CPROVER_assume(md != NULL);
    return rv;
}

/* CBMC helper functions */

/* Helper function for CBMC proofs: checks if HMAC_CTX is valid. */
bool hmac_ctx_is_valid(HMAC_CTX *ctx) {
    return ctx && ctx->is_initialized;
}

/* Helper function for CBMC proofs: checks if EVP_PKEY is valid. */
bool evp_pkey_is_valid(EVP_PKEY *pkey) {
    return pkey && (pkey->references > 0) && (pkey->ec_key == NULL || ec_key_is_valid(pkey->ec_key));
}

/* Helper function for CBMC proofs: allocates EVP_PKEY nondeterministically. */
EVP_PKEY *evp_pkey_nondet_alloc() {
    EVP_PKEY *pkey = malloc(sizeof(EVP_PKEY));
    return pkey;
}

/* Helper function for CBMC proofs: returns the reference count. */
int evp_pkey_get_reference_count(EVP_PKEY *pkey) {
    return pkey ? pkey->references : 0;
}

/* Helper function for CBMC proofs: set EC_KEY without incrementing the reference count. */
void evp_pkey_set0_ec_key(EVP_PKEY *pkey, EC_KEY *ec) {
    if (pkey) pkey->ec_key = ec;
}

/* Helper function for CBMC proofs: frees the memory regardless of the reference count. */
void evp_pkey_unconditional_free(EVP_PKEY *pkey) {
    free(pkey);
    // Does not free EC_KEY, since this is always done separately in our use cases
}

bool evp_pkey_ctx_is_valid(EVP_PKEY_CTX *ctx) {
    return ctx && (ctx->pkey == NULL || evp_pkey_is_valid(ctx->pkey));
}

bool evp_cipher_is_valid(EVP_CIPHER *cipher) {
    return cipher && (cipher->from == EVP_AES_128_GCM || cipher->from == EVP_AES_192_GCM ||
                      cipher->from == EVP_AES_256_GCM || cipher->from == EVP_AES_128_ECB);
}

bool evp_md_is_valid(EVP_MD *md) {
    return md && ((md->from == EVP_MD5 && md->md_size == 16) || (md->from == EVP_SHA1 && md->md_size == 20) ||
                  (md->from == EVP_SHA224 && md->md_size == 28) || (md->from == EVP_SHA256 && md->md_size == 32) ||
                  (md->from == EVP_SHA384 && md->md_size == 48) || (md->from == EVP_SHA512 && md->md_size == 64));
}

/* Helper function for CBMC proofs: allocates EVP_MD_CTX nondeterministically. */
EVP_MD_CTX *evp_md_ctx_nondet_alloc() {
    return malloc(sizeof(EVP_MD_CTX));
}

/* Helper function for CBMC proofs: checks if EVP_MD_CTX is initialized. */
bool evp_md_ctx_is_initialized(EVP_MD_CTX *ctx) {
    return (ctx->digest != NULL);
}

/* Helper function for CBMC proofs: returns digest size. */
size_t evp_md_ctx_get_digest_size(EVP_MD_CTX *ctx) {
    return ctx->digest->ctx_size;
}

/* Helper function for CBMC proofs: get EVP_PKEY without incrementing the reference count. */
EVP_PKEY *evp_md_ctx_get0_evp_pkey(EVP_MD_CTX *ctx) {
    return ctx ? ctx->pctx->pkey : NULL;
}

/* Helper function for CBMC proofs: set EVP_PKEY without incrementing the reference count. */
void evp_md_ctx_set0_evp_pkey(EVP_MD_CTX *ctx, EVP_PKEY *pkey) {
    if (ctx) ctx->pctx->pkey = pkey;
}

/* Helper function for CBMC proofs: frees the memory of the ctx without freeing the EVP_PKEY. */
void evp_md_ctx_shallow_free(EVP_MD_CTX *ctx) {
    free(ctx);
    // Does not free EVP_KEY, since this is always done separately in our use cases
}

void EVP_MD_CTX_set_flags(EVP_MD_CTX *ctx, int flags) {
    assert(__CPROVER_w_ok(ctx, sizeof(*ctx)));
    ctx->flags |= flags;
}

int EVP_MD_CTX_test_flags(const EVP_MD_CTX *ctx, int flags) {
    assert(__CPROVER_w_ok(ctx, sizeof(*ctx)));
    return (ctx->flags & flags);
}

int EVP_MD_CTX_copy_ex(EVP_MD_CTX *out, const EVP_MD_CTX *in) {
    assert(out != NULL);
    if (in == NULL) return 0;
    return (int)nondet_bool();
}

/**
 * https://docs.openssl.org/master/man3/EVP_EncodeInit/#description
 *
 * EVP_DecodeBlock() will decode the block of n characters of base64 data contained
 * in f and store the result in t.
 */
int EVP_DecodeBlock(unsigned char *t, const unsigned char *f, int n) {
    if (n == 0) {
        return 0;
    }
    if (nondet_bool()) {
        return -1;
    }

    /* > its length MUST be divisible by 4 */
    assert(n % 4 == 0);

    assert(__CPROVER_r_ok(f, n));

    /* > For every 4 input bytes exactly 3 output bytes will be produced */
    int written_length = n / 4 * 3;
    assert(__CPROVER_w_ok(t, written_length));

    /* > EVP_DecodeBlock() will return the length of the data decoded or -1 on error */
    return n;
}

/**
 * https://docs.openssl.org/master/man3/EVP_EncodeInit/#description
 *
 * EVP_EncodeBlock() encodes a full block of input data in f and of length n and
 * stores it in t.
 */
int EVP_EncodeBlock(unsigned char *t, const unsigned char *f, int n) {
    /* even if no data is passed in, should be able to write null terminator */
    assert(__CPROVER_w_ok(t, 1));
    if (n == 0) {
        return 0;
    }
    if (nondet_bool()) {
        return -1;
    }

    assert(__CPROVER_r_ok(f, n));

    /* > For every 3 bytes of input provided 4 bytes of output data will be produced. */
    int written_length = n / 3 * 4;
    /* > If n is not divisible by 3 then the block is encoded as a final block of
     * > data and the output is padded such that it is always divisible by 4 */
    written_length += (n % 3 != 0) ? 4 : 0;
    /* > Additionally a NUL terminator character will be added. */
    written_length += 1;
    assert(__CPROVER_w_ok(t, written_length));
    /* > The length of the data generated without the NUL terminator is returned
     * > from the function. */
    return written_length - 1;
}
