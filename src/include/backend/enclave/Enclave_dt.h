
#ifndef ENCLAVE_DT_H
#define ENCLAVE_DT_H

#define SGX_SUCCESS 1

typedef unsigned int sgx_status_t;



void initSOE(const char* tName, const char* iName, int tNBlocks, int nBlocks, unsigned int tOid, unsigned int iOid, unsigned int functionOid, char* attrDesc, unsigned int attrDescLength);

void insert(const char* heapTuple, unsigned int tupleSize,  const char* datum, unsigned int datumSize);

void insertHeap(const char* heapTuple, unsigned int tupleSize);

int getTuple(unsigned int opmode, const char* key, int scanKeySize, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen);


void closeSoe();

extern void oc_logger(const char* str);
extern sgx_status_t outFileInit(const char* filename, const char* pages, unsigned int nblocks, unsigned int blocksize, int pagesSize);
extern sgx_status_t outFileRead(char* page, const char* filename, int blkno, int pageSize);
extern sgx_status_t outFileWrite(const char* block, const char* filename, int oblkno, int pageSize);
extern sgx_status_t outFileClose(const char* filename);

#endif				/* ENCLAVE_DT_H */