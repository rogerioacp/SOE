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
#include <openssl/hmac.h>


unsigned char *key = (unsigned char *) "01234567890123456789012345678901";
unsigned int keylen = 34*sizeof(char);
#endif


//Example from https://www.openssl.org/docs/manmaster/man3/EVP_DigestInit.html
void prf(unsigned int level, unsigned int offset, unsigned int counter,  unsigned char *token)
{
#ifdef NPRF
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
    /*Use an HMAC-SHA256 to generate the cryptographic token*/
    
    const int msg[3];
    int result = -1;
    int md_len;
    

    msg[0] = level;
    msg[1] = offset;
    msg[2] = counter;

    EVP_MD_CTX* ctx = NULL;

	if (!(ctx = EVP_MD_CTX_new())){
		selog(ERROR, "could not inittialize hmac context");
        abort();
    }


    if(1 != EVP_DigestInit_ex(md,EVP_sha256(), NULL)){
        selog(ERROR, "Could not initalize SHA256");
        abort();
    }

    if(1 != EVP_DigestUpdate(md,(const unsigned char*) msg, sizeof(int)*3)){
        selog(ERROR, "Could not update digest");
        abort();
    }

    if(1 != EVP_DigestFinal_ex(md, token, &md_len)){
        selog(ERROR, "Could not finalize md");
        abort();

    }

    selog(DEBUG, "digest size is %d", md_len);

    EVP_MD_CTX_free(md);

    /*printf("Digest is: ");
    for (i = 0; i < md_len; i++)
         printf("%02x", md_value[i]);
    printf("\n");
*/
   


    /*HMAC_CTX *ctx;


   
    
      
#endif

}
