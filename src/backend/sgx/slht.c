/*-------------------------------------------------------------------------
 *
 * slht.c
 *	  SOE implementation of the slht operations on SGX.
 *
 *
 * IDENTIFICATION
 *	  src/backend/sgx/slht.c
 *
 *-------------------------------------------------------------------------
 */


#include "backend/soe.h"
#include <sgx_tcrypto.h>

sgx_aes_gcm_128bit_key_t key = {
		0x72, 0x12, 0x8a, 0x7a, 0x17, 0x52, 0x6e, 0xbf,
        0x85, 0xd0, 0x3a, 0x62, 0x37, 0x30, 0xae, 0xad
    }

struct State {
	/* 
	 * Number of insertion tokens received by the SOE. 
	 * Corresponds to the N on the cryptographic games in the paper
	 */
	int nInsertions;
	/*
     * Current number of bucket in the SLHT according to the number of 
     * insertion tokens received.
	*/
	int nBuckets;
};


struct InitParams{
	/*Initial number of buckets with which the SLHt is initialized*/
	int nBuckets;
};


struct token{
	void* ciphertext;
	void* associatedData;
};

State state;


State Init(InitParams params){

	if(params.nBuckets < 0){
		return state;
	}

	state.nInsertions = 0;
	state.nBuckets = params.nBuckets;


	return NULL;
	//Encrypted and authenticate state.
}
