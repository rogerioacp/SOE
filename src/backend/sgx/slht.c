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
}


struct InitParams{
	/*Initial number of buckets with which the SLHt is initialized*/
	int nBuckets;
}


struct token{
	void* ciphertext;
	void* associatedData;
}



State Init(InitParams params){

	if(params.nBuckets < 0){
		return null;
	}
	State state;
	state.nInsertions = 0;
	sate.nBuckets = params.nBuckets;


	//Encrypted and authenticate state.
}
