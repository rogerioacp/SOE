/**
 * Implementation of the page encryption using openssl. This implementation 
 * is only used on the UNSAFE mode for now. In the future it can be integrated
 * with intel-openssl-sgx library.
 */

#include "soe_c.h"
#include "common/soe_pe.h"
#include "logger/logger.h"

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

unsigned char *key = (unsigned char *)"01234567890123456789012345678901";
unsigned char *iv = (unsigned char *)"0123456789012345";

//#define BUFFLEN  BLCKSZ + SGX_AESGCM_MAC_SIZE + SGX_AESGCM_IV_SIZE

void page_encryption(unsigned char *plaintext, unsigned char* ciphertext)
{

    EVP_CIPHER_CTX *ctx;
    int ciphertext_len;
    int len;

   /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        selog(ERROR, "could not create openssl context for encryption");
	
	/*
     * Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
    	selog(ERROR, "could not init encryption context");

	EVP_CIPHER_CTX_set_padding(ctx, 0);
    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, BLCKSZ))
         selog(ERROR, "could not encrypt update");
    
    ciphertext_len = len;

	/*
     * Finalize the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
    	selog(ERROR, "could not finalize encrypt");

    ciphertext_len += len;

    if(ciphertext_len != BLCKSZ){
    	selog(ERROR, "Decription plaintex length does not match");
    }
  
    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

}

void page_decryption(unsigned char* ciphertext, unsigned char* plaintext)
{
	 EVP_CIPHER_CTX *ctx;

    int len;

    int plaintext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        selog(ERROR, "could not create openssl context for decryption");

    /*
     * Initialise the decryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
    	selog(ERROR, "could not decryption context");

	EVP_CIPHER_CTX_set_padding(ctx, 0);

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary.
     */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, BLCKSZ))
         selog(ERROR, "could not decrypt update");

    plaintext_len = len;

    /*
     * Finalise the decryption. Further plaintext bytes may be written at
     * this stage.
     */
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
    	selog(ERROR, "could not finalize decrypt");

    plaintext_len += len;

    if(plaintext_len != BLCKSZ){
    	selog(ERROR, "Decription plaintex length does not match");
    }
  
    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);
}
