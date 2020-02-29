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

void initSOE(const char* tName, const char* iName, int tNBlocks, int* fanout, unsigned int fanout_size, unsigned int nlevels, int inBlocks, unsigned int tOid, unsigned int iOid, unsigned int functionOid, unsigned int indexHandler, char* pg_attr_desc, unsigned int pgDescSize);
void initFSOE(const char* tName, const char* iName, int tNBlocks, int* fanout, unsigned int fanout_size, unsigned int nlevels, unsigned int tOid, unsigned int iOid, char* pg_attr_desc, unsigned int pgDescSize);
void addIndexBlock(char* block, unsigned int blockSize, unsigned int offset, unsigned int level);
void addHeapBlock(char* block, unsigned int blockSize, unsigned int blkno);
void insert(const char* heapTuple, unsigned int tupleSize, char* datum, unsigned int datumSize);
int getTuple(unsigned int opmode, unsigned int opoid, const char* scanKey, int scanKeySize, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen);
void insertHeap(const char* heapTuple, unsigned int tupleSize);

sgx_status_t SGX_CDECL oc_logger(const char* str);
sgx_status_t SGX_CDECL outFileInit(const char* filename, const char* pages, unsigned int nblocks, unsigned int blocksize, int pagesSize, int initOffset);
sgx_status_t SGX_CDECL outFileRead(char* page, const char* filename, int blkno, int pageSize);
sgx_status_t SGX_CDECL outFileWrite(const char* block, const char* filename, int oblkno, int pageSize);
sgx_status_t SGX_CDECL outFileClose(const char* filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
