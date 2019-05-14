
#include "logger/logger.h"

#ifdef UNSAFE
#include "Enclave_dt.h"
#else
#include "Enclave_t.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <oram/logger.h>


void logger(int level, const char* message, ...){
    //char* buf = (char*) malloc(sizeof(char)*BUFSIZE);
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    //int written = 0;
    //int result;
    va_list ap;
    va_start(ap, message);
    /*written = vsnprintf(buf, BUFSIZE, message, ap);
    if(written < BUFSIZE){
        buf = realloc(buf, sizeof(char)*written);
        memset(buf, 0, written);
    }*/

    vsnprintf(buf, BUFSIZE, message, ap);
    va_end(ap);
    oc_logger(buf);
}

void selog(int level, const char* message, ...){
    //char* buf = (char*) malloc(sizeof(char)*BUFSIZE);
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    //int written = 0;
    //int result;
    va_list ap;
    va_start(ap, message);
    /*written = vsnprintf(buf, BUFSIZE, message, ap);
    if(written < BUFSIZE){
    	buf = realloc(buf, sizeof(char)*written);
    	memset(buf, 0, written);
    }*/

	vsnprintf(buf, BUFSIZE, message, ap);
    va_end(ap);
    oc_logger(buf);
}
