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
#include <sodium.h>

unsigned char *pkey = NULL; //= (unsigned char *) "01234567890123456789012345678901";
unsigned int keylen = 34*sizeof(char);
#endif


void prf(unsigned int level, unsigned int offset, unsigned int counter,  unsigned char *token)
{

    selog(DEBUG1, "prf for level %d offset %d  counter %d", level, offset, counter);
#ifdef PRF
    /*Use an HMAC-SHA256 to generate the cryptographic token
     * Example from https://www.openssl.org/docs/manmaster/man3/EVP_DigestInit.html
     */

    int msgc[3];
    int msgn[3];

    int res1[8];
    int res2[8];

    if(pkey == NULL){
        //selog(DEBUG1, "Init prf key");
        pkey = (unsigned char*) malloc(crypto_auth_hmacsha512_KEYBYTES);
        crypto_auth_hmacsha512_keygen(pkey);
    }


    msgc[0] = level;
    msgc[1] = offset;
    msgc[2] = counter;

    msgn[0] = level;
    msgn[1] = offset;
    msgn[2] = counter+1;

    crypto_auth_hmacsha256((unsigned char*) res1, (const unsigned char*) msgc, sizeof(int)*3, pkey);
    crypto_auth_hmacsha256((unsigned char*) res2, (const unsigned char*) msgn, sizeof(int)*3, pkey);
    
    //token = res1[0];
    //token+sizeof(unsignedint) = res2[0];
    //token + 2*sizeof(unsigned int) = res1[1];
    //token + 3*sizeof(unsigned int) = res2[1];
    memcpy(token, &res1[0], sizeof(unsigned int));
    memcpy(token + sizeof(unsigned int), &res2[0], sizeof(unsigned int));
    memcpy(token + 2*sizeof(unsigned int), &res1[1], sizeof(unsigned int));
    memcpy(token + 3*sizeof(unsigned int), &res2[1], sizeof(unsigned int));


    //selog(DEBUG1, "res is %d with values %d %d %d %d", (unsigned int) token[0], (unsigned int) token[1], (unsigned int) token[2], (unsigned int) token[3]);
    
#else
	/* If we are not generating tokens with a PRF just copy the counter */

    int next = counter + 1;
    memcpy(token, &counter, sizeof(unsigned int));
    memcpy(token + sizeof(unsigned int), &next, sizeof(unsigned int));
    memcpy(token + 2*sizeof(unsigned int), &counter, sizeof(unsigned int));
    memcpy(token + 3*sizeof(unsigned int), &next, sizeof(unsigned int));
    
#endif


}
