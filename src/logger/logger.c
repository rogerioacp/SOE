
#include "logger/logger.h"
#include "Enclave_t.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void selog(int level, const char* message, ...){
    char* buf = (char*) malloc(sizeof(char)*BUFSIZE);
    memset(buf, 0, BUFSIZE);
    int written = 0;
    int result;
    va_list ap;
    va_start(ap, message);
    written = vsnprintf(buf, BUFSIZE, message, ap);
    if(written < BUFSIZE){
    	buf = realloc(buf, sizeof(char)*written);
    	memset(buf, 0, written);
    }

	vsnprintf(buf, BUFSIZE+written, message, ap);
    va_end(ap);

    logger(buf);
    free(buf);
}
