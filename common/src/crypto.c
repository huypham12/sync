#include "../include/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define AES_KEY_SIZE 32 // 256 bits
#define AES_BLOCK_SIZE 16
#define BUF_SIZE 4096

static unsigned char aes_key[AES_KEY_SIZE];
static int key_loaded = 0;

int crypto_init(const char *key_file_path) {
    FILE *f = fopen(key_file_path, "rb");
    if (!f) {
        perror("Failed to open key file");
        return -1;
    }
    size_t read_bytes = fread(aes_key, 1, AES_KEY_SIZE, f);
    fclose(f);
    
    if (read_bytes < AES_KEY_SIZE) {
        fprintf(stderr, "Warning: Key file is smaller than %d bytes. Padding with 0s.\n", AES_KEY_SIZE);
        memset(aes_key + read_bytes, 0, AES_KEY_SIZE - read_bytes);
    }
    key_loaded = 1;
    return 0;
}

int encrypt_file(const char *input_path, const char *output_path) {
    if (!key_loaded) {
        fprintf(stderr, "Crypto not initialized. Call crypto_init first.\n");
        return -1;
    }

    FILE *f_in = fopen(input_path, "rb");
    if (!f_in) {
        perror("Cannot open input file");
        return -1;
    }

    FILE *f_out = fopen(output_path, "wb");
    if (!f_out) {
        perror("Cannot open output file");
        fclose(f_in);
        return -1;
    }

    // Generate random IV
    unsigned char iv[AES_BLOCK_SIZE];
    if (!RAND_bytes(iv, AES_BLOCK_SIZE)) {
        fprintf(stderr, "Failed to generate random IV.\n");
        fclose(f_in); fclose(f_out);
        return -1;
    }

    // Write IV to the beginning of the encrypted file
    fwrite(iv, 1, AES_BLOCK_SIZE, f_out);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Failed to create EVP_CIPHER_CTX\n");
        fclose(f_in); fclose(f_out);
        return -1;
    }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_key, iv)) {
        fprintf(stderr, "EVP_EncryptInit_ex failed\n");
        EVP_CIPHER_CTX_free(ctx); fclose(f_in); fclose(f_out);
        return -1;
    }

    unsigned char in_buf[BUF_SIZE];
    unsigned char out_buf[BUF_SIZE + AES_BLOCK_SIZE];
    int in_len, out_len;

    while ((in_len = fread(in_buf, 1, BUF_SIZE, f_in)) > 0) {
        if (1 != EVP_EncryptUpdate(ctx, out_buf, &out_len, in_buf, in_len)) {
            fprintf(stderr, "EVP_EncryptUpdate failed\n");
            EVP_CIPHER_CTX_free(ctx); fclose(f_in); fclose(f_out);
            return -1;
        }
        fwrite(out_buf, 1, out_len, f_out);
    }

    if (1 != EVP_EncryptFinal_ex(ctx, out_buf, &out_len)) {
        fprintf(stderr, "EVP_EncryptFinal_ex failed\n");
        EVP_CIPHER_CTX_free(ctx); fclose(f_in); fclose(f_out);
        return -1;
    }
    fwrite(out_buf, 1, out_len, f_out);

    EVP_CIPHER_CTX_free(ctx);
    fclose(f_in);
    fclose(f_out);
    return 0;
}

int decrypt_file(const char *input_path, const char *output_path) {
    if (!key_loaded) {
        fprintf(stderr, "Crypto not initialized. Call crypto_init first.\n");
        return -1;
    }

    FILE *f_in = fopen(input_path, "rb");
    if (!f_in) {
        perror("Cannot open input file");
        return -1;
    }

    FILE *f_out = fopen(output_path, "wb");
    if (!f_out) {
        perror("Cannot open output file");
        fclose(f_in);
        return -1;
    }

    // Read IV from the beginning of the file
    unsigned char iv[AES_BLOCK_SIZE];
    if (fread(iv, 1, AES_BLOCK_SIZE, f_in) != AES_BLOCK_SIZE) {
        fprintf(stderr, "Failed to read IV from file.\n");
        fclose(f_in); fclose(f_out);
        return -1;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Failed to create EVP_CIPHER_CTX\n");
        fclose(f_in); fclose(f_out);
        return -1;
    }

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_key, iv)) {
        fprintf(stderr, "EVP_DecryptInit_ex failed\n");
        EVP_CIPHER_CTX_free(ctx); fclose(f_in); fclose(f_out);
        return -1;
    }

    unsigned char in_buf[BUF_SIZE];
    unsigned char out_buf[BUF_SIZE + AES_BLOCK_SIZE];
    int in_len, out_len;

    while ((in_len = fread(in_buf, 1, BUF_SIZE, f_in)) > 0) {
        if (1 != EVP_DecryptUpdate(ctx, out_buf, &out_len, in_buf, in_len)) {
            fprintf(stderr, "EVP_DecryptUpdate failed\n");
            EVP_CIPHER_CTX_free(ctx); fclose(f_in); fclose(f_out);
            return -1;
        }
        fwrite(out_buf, 1, out_len, f_out);
    }

    if (1 != EVP_DecryptFinal_ex(ctx, out_buf, &out_len)) {
        fprintf(stderr, "EVP_DecryptFinal_ex failed\n");
        EVP_CIPHER_CTX_free(ctx); fclose(f_in); fclose(f_out);
        return -1;
    }
    fwrite(out_buf, 1, out_len, f_out);

    EVP_CIPHER_CTX_free(ctx);
    fclose(f_in);
    fclose(f_out);
    return 0;
}

int compute_sha256(const char *file_path, char *output_buffer) {
    FILE *f = fopen(file_path, "rb");
    if (!f) return -1;

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestInit_ex(mdctx, md, NULL);

    unsigned char buffer[BUF_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUF_SIZE, f)) != 0) {
        EVP_DigestUpdate(mdctx, buffer, bytes);
    }

    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);
    fclose(f);

    for (unsigned int i = 0; i < md_len; i++) {
        sprintf(&output_buffer[i * 2], "%02x", md_value[i]);
    }
    output_buffer[md_len * 2] = '\0';
    return 0;
}
