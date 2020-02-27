/*-------------------------------------------------------------------------
 *
 * soe_prf.c
 *	High-level interface that abstracts the underlying PRFs used for
 *	the generation of the ORAM pmap.
 *
 * identification
 *	  src/common/soe_prf.c
 *
 *-------------------------------------------------------------------------
 */
#include "soe_c.h"
#include "common/soe_prf.h"
#include "logger/logger.h"

#ifndef CPAGES

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

unsigned char *key = (unsigned char *) "01234567890123456789012345678901";
unsigned char *iv = (unsigned char *) "0123456789012345";
#endif

void prf(unsigned int level, unsigned int offset, unsigned int counter,  unsigned char *token)
{
#ifdef CPAGES
	/* If we are not encrypting anything just generate two random ints. Its all
     * we need*/
    int next = counter + 1;
    //int r = 0;
    //for(index  = 0; index < 2; index++){
        //r = getRandomInt();
    //Currently we only need 4 integer, 2 for the leaf and 2 for  a partition.
    // 1 token for the current loction and a token for the next location.
    memcpy(token, &counter, sizeof(unsigned int));
    memcpy(token + sizeof(unsigned int), &next, sizeof(unsigned int));
    memcpy(token + 2*sizeof(unsigned int), &counter, sizeof(unsigned int));
    memcpy(token + 3*sizeof(unsigned int), &next, sizeof(unsigned int));
    //}
     
    //token[0] = counter;
    //token[1] = next;
    //token[2] = counter;
    //token[3] = next;

    

#else
    /*Use an AES CBC to generate the cryptographic token*/
    int len = 3 * sizeof(unsigned int);
    char input[len];


    EVP_CIPHER_CTX *ctx;
	int			ciphertext_len;
	int			clen;

    len[0] = level;
    len[1] = offset;
    len[2] = counter;


	/* Create and initialise the context */
	if (!(ctx = EVP_CIPHER_CTX_new()))
		selog(ERROR, "could not create openssl context for encryption");

	/*
	 * Initialise the encryption operation. IMPORTANT - ensure you use a key
	 * and IV size appropriate for your cipher In this example we are using
	 * 256 bit AES (i.e. a 256 bit key). The IV size for *most* modes is the
	 * same as the block size. For AES this is 128 bits
	 */
	if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
		selog(ERROR, "could not init encryption context");

	EVP_CIPHER_CTX_set_padding(ctx, 1);

	/*
	 * Provide the message to be encrypted, and obtain the encrypted output.
	 * EVP_EncryptUpdate can be called multiple times if necessary
	 */
	if (1 != EVP_EncryptUpdate(ctx, token, &clen, &input, len))
		selog(ERROR, "could not encrypt update");

	ciphertext_len = clen;

	/*
	 * Finalize the encryption. Further ciphertext bytes may be written at
	 * this stage.
	 */
	if (1 != EVP_EncryptFinal_ex(ctx, token + clen, &clen))
		selog(ERROR, "could not finalize encrypt");

	ciphertext_len += clen;

	if (ciphertext_len != len)
	{
		selog(ERROR, "Decription plaintex length does not match");
	}

	/* Clean up */
	EVP_CIPHER_CTX_free(ctx);
#endif    

}
