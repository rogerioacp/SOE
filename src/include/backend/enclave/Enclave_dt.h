
#ifndef ENCLAVE_DT_H
#define ENCLAVE_DT_H

#define SGX_SUCCESS 1

typedef unsigned int sgx_status_t;



void initSOE(const char* tName, const char* iName, int tNBlocks, int nBlocks, unsigned int tOid, unsigned int iOid);
void insert(const char* heapTuple, unsigned int tupleSize);
char* getTuple(const char* scanKey, int scanKeySize);
void insertHeap(const char* heapTuple, unsigned int tupleSize);
void getTupleTID(unsigned int blkno, unsigned int offnum, char* tuple, unsigned int tupleLen);


extern void oc_logger(const char* str);
extern sgx_status_t outFileInit(const char* filename, const char* pages, unsigned int nblocks, unsigned int blocksize, int pagesSize);
extern sgx_status_t outFileRead(char* page, const char* filename, int blkno, int pageSize);
extern sgx_status_t outFileWrite(const char* block, const char* filename, int oblkno, int pageSize);
extern sgx_status_t outFileClose(const char* filename);

#endif				/* ENCLAVE_DT_H */