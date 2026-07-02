#include "netnemesis.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

// GCM authentication tag (16 bytes = 128 bits)
#define GCM_TAG_LEN 16

CryptoManager::CryptoManager() {
    memset(key, 0, AES_KEY_SIZE);
    memset(iv, 0, AES_IV_SIZE);
}

void CryptoManager::deriveKey(const std::string &password) {
    // Derive a key from the password using simple SHA-256
    // In production use PBKDF2 or Argon2
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return;
    
    unsigned char hash[32];
    unsigned int hash_len = 0;
    
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, password.c_str(), password.length());
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);
    
    memcpy(key, hash, AES_KEY_SIZE);
    
    // Generate a random IV for each session
    RAND_bytes(iv, AES_IV_SIZE);
}

std::string CryptoManager::base64Encode(const unsigned char *data, int len) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, len);
    BIO_flush(b64);
    
    BUF_MEM *bufferPtr;
    BIO_get_mem_ptr(b64, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(b64);
    
    return result;
}

std::vector<unsigned char> CryptoManager::base64Decode(const std::string &encoded) {
    std::vector<unsigned char> decoded(encoded.length());
    
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new_mem_buf(encoded.c_str(), encoded.length());
    b64 = BIO_push(b64, mem);
    
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    int len = BIO_read(b64, decoded.data(), encoded.length());
    BIO_free_all(b64);
    
    if (len < 0) {
        return std::vector<unsigned char>();
    }
    
    decoded.resize(len);
    return decoded;
}

std::string CryptoManager::encrypt(const std::string &plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    
    std::vector<unsigned char> ciphertext(plaintext.length() + GCM_TAG_LEN);
    int len;
    int ciphertext_len;
    
    // Initialize encryption
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Set IV length
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_IV_SIZE, NULL)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Initialize with key and IV
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Encrypt plaintext
    if (!EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                          (unsigned char*)plaintext.c_str(), plaintext.length())) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    ciphertext_len = len;
    
    // Finalize
    if (!EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    ciphertext_len += len;
    
    // Obtain the authentication tag
    unsigned char tag[GCM_TAG_LEN];
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LEN, tag)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Formato: IV (16 byte) + Tag (16 byte) + Ciphertext
    std::vector<unsigned char> result;
    result.reserve(AES_IV_SIZE + GCM_TAG_LEN + ciphertext_len);
    result.insert(result.end(), iv, iv + AES_IV_SIZE);
    result.insert(result.end(), tag, tag + GCM_TAG_LEN);
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    
    return base64Encode(result.data(), result.size());
}

std::string CryptoManager::decrypt(const std::string &ciphertext_b64) {
    auto data = base64Decode(ciphertext_b64);
    if (data.size() < AES_IV_SIZE + GCM_TAG_LEN) {
        return ""; // Insufficient data
    }
    
    // Extract IV, tag, and ciphertext
    unsigned char recv_iv[AES_IV_SIZE];
    unsigned char tag[GCM_TAG_LEN];
    memcpy(recv_iv, data.data(), AES_IV_SIZE);
    memcpy(tag, data.data() + AES_IV_SIZE, GCM_TAG_LEN);
    
    std::vector<unsigned char> ciphertext(data.begin() + AES_IV_SIZE + GCM_TAG_LEN, data.end());
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    
    std::vector<unsigned char> plaintext(ciphertext.size());
    int len;
    int plaintext_len;
    
    // Initialize
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Set IV length
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_IV_SIZE, NULL)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Initialize with the received key and IV
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, recv_iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Decrypt
    if (!EVP_DecryptUpdate(ctx, plaintext.data(), &len, 
                          ciphertext.data(), ciphertext.size())) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    plaintext_len = len;
    
    // Set the tag for verification
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_LEN, tag)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    // Verify and finalize
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret > 0) {
        plaintext_len += len;
        return std::string((char*)plaintext.data(), plaintext_len);
    }
    
    // Decryption failed (invalid tag = corrupted or tampered data)
    return "";
}