/**
 * Implementation of the page encryption using sgx tcrypto. 
 * This is used in normal mode where the SOE is loaded to
 * an SGX enclave.
 */

#include "soe_c.h"
#include "common/soe_pe.h"
#include "logger/logger.h"
#include "ippcp.h"
#include <stdlib.h>

#define KEY_SIZE 16

#define IV 16

Ipp8u key[] = "\x00\x01\x02\x03\x04\x05\x06\x07"
        "\x08\x09\x10\x11\x12\x13\x14\x15";

Ipp8u iv[] = "\xff\xee\xdd\xcc\xbb\xaa\x99\x88"
   			 "\x77\x66\x55\x44\x33\x22\x11\x00";



void page_encryption(unsigned char *plaintext, unsigned char* ciphertext)
{
	IppStatus error_code = ippStsNoErr;
    IppsAESSpec* ptr_ctx = NULL;
    int ctx_size = 0;

    if(plaintext == NULL){
    	selog(ERROR, "input page to encrypt is NULL");
    }

    error_code = ippsAESGetSize(&ctx_size);

    if (error_code != ippStsNoErr)
    {
    	selog(ERROR, "Unexpected error on page_encryption");
    }

    ptr_ctx = (IppsAESSpec*) malloc(ctx_size);

    if(ptr_ctx == NULL){
    	selog(ERROR, "Out of memory on page encryption");
    }

    error_code = ippsAESInit(key, KEY_SIZE, ptr_ctx, ctx_size);

    if(error_code != ippStsNoErr){
    	memset(ptr_ctx, 0, ctx_size);
    	free(ptr_ctx);
    	selog(ERROR, "Unexpected error when initializing ippsAES");
    }

    error_code = ippsAESEncryptCBC((uint8_t*) plaintext, (uint8_t*) ciphertext,BLCKSZ, ptr_ctx, (uint8_t*) iv);

    if(error_code != ippStsNoErr){
    	memset(ptr_ctx, 0, ctx_size);
    	free(ptr_ctx);
    	selog(ERROR, "Unexpected error when encrypting with CBC");
    }

   	memset(ptr_ctx, 0, ctx_size);
    free(ptr_ctx);
}

void page_decryption(unsigned char* ciphertext, unsigned char* plaintext){
    
    IppStatus error_code = ippStsNoErr;
    IppsAESSpec* ptr_ctx = NULL;
    int ctx_size = 0;


    if(ciphertext == NULL){
    	selog(ERROR, "input page to decrypt is NULL");
    }

    error_code = ippsAESGetSize(&ctx_size);

    if (error_code != ippStsNoErr)
    {
    	selog(ERROR, "Unexpected error on page_encryption");
    }

    ptr_ctx = (IppsAESSpec*)malloc(ctx_size);
    
    if (ptr_ctx == NULL)
    {
    	selog(ERROR, "Out of memory on page encryption");
    }

    error_code = ippsAESInit(key, KEY_SIZE, ptr_ctx, ctx_size);

    if(error_code != ippStsNoErr){
    	memset(ptr_ctx, 0, ctx_size);
    	free(ptr_ctx);
    	selog(ERROR, "Unexpected error when initializing ippsAES on decrypt");
    }

    error_code = ippsAESDecryptCBC(ciphertext, plaintext, BLCKSZ, ptr_ctx, (uint8_t*) iv);

     if(error_code != ippStsNoErr){
    	memset(ptr_ctx, 0, ctx_size);
    	free(ptr_ctx);
    	selog(ERROR, "Unexpected error when encrypting with CBC");
    }

    memset(ptr_ctx, 0, ctx_size);
    free(ptr_ctx);
}
