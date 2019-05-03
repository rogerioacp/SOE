#ifndef ENCLAVE_T_H__
#define ENCLAVE_T_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include "sgx_edger8r.h" /* for sgx_ocall etc. */


#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

void initSOE(const char* tName, const char* iName, int tNBlocks, int nBlocks);
void insert(const char* heapTuple);
char* getTuple(const char* scanKey);

sgx_status_t SGX_CDECL logger(const char* str);
sgx_status_t SGX_CDECL outFileInit(const char* filename, unsigned int nblocks, unsigned int blocksize);
sgx_status_t SGX_CDECL outFileRead(char* block, const char* filename, int blkno);
sgx_status_t SGX_CDECL outFileWrite(char* block, const char* filename, int oblkno);
sgx_status_t SGX_CDECL outFileClose(const char* filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
