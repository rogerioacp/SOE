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

void initSOE(const char* tName, const char* iName, int tNBlocks, int nBlocks, unsigned int tOid, unsigned int iOid);
void insert(const char* heapTuple, unsigned int tupleSize);
char* getTuple(const char* scanKey, int scanKeySize);
void insertHeap(const char* heapTuple, unsigned int tupleSize);
void getTupleTID(unsigned int blkno, unsigned int offnum, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen);

sgx_status_t SGX_CDECL oc_logger(const char* str);
sgx_status_t SGX_CDECL outFileInit(const char* filename, const char* pages, unsigned int nblocks, unsigned int blocksize, int pagesSize);
sgx_status_t SGX_CDECL outFileRead(char* page, const char* filename, int blkno, int pageSize);
sgx_status_t SGX_CDECL outFileWrite(const char* block, const char* filename, int oblkno, int pageSize);
sgx_status_t SGX_CDECL outFileClose(const char* filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
