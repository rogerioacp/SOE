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

#ifdef PRF
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>

unsigned char *key = (unsigned char *) "01234567890123456789012345678901";
unsigned int keylen = 34*sizeof(char);
#endif


void prf(unsigned int level, unsigned int offset, unsigned int counter,  unsigned char *token)
{
#ifdef PRF
    /*Use an HMAC-SHA256 to generate the cryptographic token
     * Example from https://www.openssl.org/docs/manmaster/man3/EVP_DigestInit.html
     */

    int msg[3];
    unsigned int md_len;
    

    msg[0] = level;
    msg[1] = offset;
    msg[2] = counter;

    EVP_MD_CTX *ctx = NULL;

	if (!(ctx = EVP_MD_CTX_new())){
		selog(ERROR, "could not inittialize hmac context");
        abort();
    }


    if(1 != EVP_DigestInit_ex(ctx,EVP_sha256(), NULL)){
        selog(ERROR, "Could not initalize SHA256");
        abort();
    }

    if(1 != EVP_DigestUpdate(ctx,(const unsigned char*) msg, sizeof(int)*3)){
        selog(ERROR, "Could not update digest");
        abort();
    }

    if(1 != EVP_DigestFinal_ex(ctx, token, &md_len)){
        selog(ERROR, "Could not finalize md");
        abort();

    }

    EVP_MD_CTX_free(ctx);
    
#else
	/* If we are not generating tokens with a PRF just copy the counter */

    int next = counter + 1;
    memcpy(token, &counter, sizeof(unsigned int));
    memcpy(token + sizeof(unsigned int), &next, sizeof(unsigned int));
    memcpy(token + 2*sizeof(unsigned int), &counter, sizeof(unsigned int));
    memcpy(token + 3*sizeof(unsigned int), &next, sizeof(unsigned int));
    
#endif


}
