/**
* File used to store the context of the pseudo random generator and generate
* new unsigned 32 byte numbers.
*
*/

#include <ippcp.h>

IppsPRNGState* pCtx;

IppStatus initializePRNG(){
	int PRNSize = 0;
    IPPStatus error_code;

	error_code = ippsPRNGGetSize(&PRNSize);

	if(error_code != ippStsNoErr){
		return error_code
	}

	IppsPRNGState* pCtx = (IppsPRNGState *) malloc(ctxSize);

	if(pCtx == NULL)
        return ippStsMemAllocErr;

 	error_code = ippsPRNGInit(160, pCtx);

     if(error_code != ippStsNoErr)
    {
        free(pCtx);
        return error_code;
    }

    return errorCode;
}


IppStatus nextPRN(uint32_t* destIV, int nbits){
    IppStatus error_code;

    //Check if the prng context has been initialized.
    if(pCtx == NULL)
        return ippStsContextMatchErr

    error_code = ippsPRNGen(destIV, nbits, pCtx);
    if(error_code != ippStsNoErr)
    {
        free(pCtx);
        return error_code;
    }
    return error_code;
}


void destroyContext(){
	free(pCtx);
}


