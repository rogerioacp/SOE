
#include <oram/orandom.h>

#ifdef UNSAFE
#include <stdlib.h>
#include <time.h>
#else
#include "sgx_trts.h"
#endif

#include "logger/logger.h"

unsigned int getRandomInt(void){

	#ifdef UNSAFE
		return random();
	#else
		unsigned int val;
		sgx_read_rand(&val, sizeof(unsigned int));
	return val;
	#endif
}