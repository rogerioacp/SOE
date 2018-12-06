/*-------------------------------------------------------------------------
 *
 *  prng.h
 *	  The public API for the pseudo random number generator (PRNG). 
 *    This API defines the function to initialize a PRNG context, request 
 *    random unsigned 32 byte numbers and clear the context. It is a common 
 *    utilitity used to encrypted data on the client and the SOE.
 *
 *     COPYRIGHT.
 *
 * src/include/soe.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PRNG_H
#define PRNG_H

#include <ippcp.h>

//Initialize the pseudo random context.
IppStatus initializePRNG();

// Generate a new random number on the input address.
IppStatus nextPRN(uint32_t* destIV, int nbits);

//Clear the pseudo random generator context.
void destroyContext();

#endif