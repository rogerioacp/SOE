#ifndef ENCLAVE_U_H__
#define ENCLAVE_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_satus_t etc. */


#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OC_LOGGER_DEFINED__
#define OC_LOGGER_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, oc_logger, (const char* str));
#endif
#ifndef OUTFILEINIT_DEFINED__
#define OUTFILEINIT_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, outFileInit, (const char* filename, const char* pages, unsigned int nblocks, unsigned int blocksize, int pagesSize));
#endif
#ifndef OUTFILEREAD_DEFINED__
#define OUTFILEREAD_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, outFileRead, (char* page, const char* filename, int blkno, int pageSize));
#endif
#ifndef OUTFILEWRITE_DEFINED__
#define OUTFILEWRITE_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, outFileWrite, (const char* block, const char* filename, int oblkno, int pageSize));
#endif
#ifndef OUTFILECLOSE_DEFINED__
#define OUTFILECLOSE_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, outFileClose, (const char* filename));
#endif

sgx_status_t initSOE(sgx_enclave_id_t eid, const char* tName, const char* iName, int tNBlocks, int nBlocks, unsigned int tOid, unsigned int iOid, unsigned int functionOid, char* pg_attr_desc, unsigned int pgDescSize);
sgx_status_t insert(sgx_enclave_id_t eid, const char* heapTuple, unsigned int tupleSize, const char* datum, unsigned int datumSize);
sgx_status_t getTuple(sgx_enclave_id_t eid, int* retval, unsigned int opmode, const char* scanKey, int scanKeySize, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen);
sgx_status_t insertHeap(sgx_enclave_id_t eid, const char* heapTuple, unsigned int tupleSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
