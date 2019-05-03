#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)


typedef struct ms_initSOE_t {
	const char* ms_tName;
	size_t ms_tName_len;
	const char* ms_iName;
	size_t ms_iName_len;
	int ms_tNBlocks;
	int ms_nBlocks;
} ms_initSOE_t;

typedef struct ms_insert_t {
	const char* ms_heapTuple;
	size_t ms_heapTuple_len;
} ms_insert_t;

typedef struct ms_getTuple_t {
	char* ms_retval;
	const char* ms_scanKey;
	size_t ms_scanKey_len;
} ms_getTuple_t;

typedef struct ms_logger_t {
	const char* ms_str;
} ms_logger_t;

typedef struct ms_outFileInit_t {
	const char* ms_filename;
	unsigned int ms_nblocks;
	unsigned int ms_blocksize;
} ms_outFileInit_t;

typedef struct ms_outFileRead_t {
	char* ms_block;
	const char* ms_filename;
	int ms_blkno;
} ms_outFileRead_t;

typedef struct ms_outFileWrite_t {
	char* ms_block;
	const char* ms_filename;
	int ms_oblkno;
} ms_outFileWrite_t;

typedef struct ms_outFileClose_t {
	const char* ms_filename;
} ms_outFileClose_t;

static sgx_status_t SGX_CDECL sgx_initSOE(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_initSOE_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_initSOE_t* ms = SGX_CAST(ms_initSOE_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_tName = ms->ms_tName;
	size_t _len_tName = ms->ms_tName_len ;
	char* _in_tName = NULL;
	const char* _tmp_iName = ms->ms_iName;
	size_t _len_iName = ms->ms_iName_len ;
	char* _in_iName = NULL;

	CHECK_UNIQUE_POINTER(_tmp_tName, _len_tName);
	CHECK_UNIQUE_POINTER(_tmp_iName, _len_iName);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_tName != NULL && _len_tName != 0) {
		_in_tName = (char*)malloc(_len_tName);
		if (_in_tName == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_tName, _len_tName, _tmp_tName, _len_tName)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

		_in_tName[_len_tName - 1] = '\0';
		if (_len_tName != strlen(_in_tName) + 1)
		{
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}
	if (_tmp_iName != NULL && _len_iName != 0) {
		_in_iName = (char*)malloc(_len_iName);
		if (_in_iName == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_iName, _len_iName, _tmp_iName, _len_iName)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

		_in_iName[_len_iName - 1] = '\0';
		if (_len_iName != strlen(_in_iName) + 1)
		{
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

	initSOE((const char*)_in_tName, (const char*)_in_iName, ms->ms_tNBlocks, ms->ms_nBlocks);
err:
	if (_in_tName) free(_in_tName);
	if (_in_iName) free(_in_iName);

	return status;
}

static sgx_status_t SGX_CDECL sgx_insert(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_insert_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_insert_t* ms = SGX_CAST(ms_insert_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_heapTuple = ms->ms_heapTuple;
	size_t _len_heapTuple = ms->ms_heapTuple_len ;
	char* _in_heapTuple = NULL;

	CHECK_UNIQUE_POINTER(_tmp_heapTuple, _len_heapTuple);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_heapTuple != NULL && _len_heapTuple != 0) {
		_in_heapTuple = (char*)malloc(_len_heapTuple);
		if (_in_heapTuple == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_heapTuple, _len_heapTuple, _tmp_heapTuple, _len_heapTuple)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

		_in_heapTuple[_len_heapTuple - 1] = '\0';
		if (_len_heapTuple != strlen(_in_heapTuple) + 1)
		{
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

	insert((const char*)_in_heapTuple);
err:
	if (_in_heapTuple) free(_in_heapTuple);

	return status;
}

static sgx_status_t SGX_CDECL sgx_getTuple(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_getTuple_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_getTuple_t* ms = SGX_CAST(ms_getTuple_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_scanKey = ms->ms_scanKey;
	size_t _len_scanKey = ms->ms_scanKey_len ;
	char* _in_scanKey = NULL;

	CHECK_UNIQUE_POINTER(_tmp_scanKey, _len_scanKey);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_scanKey != NULL && _len_scanKey != 0) {
		_in_scanKey = (char*)malloc(_len_scanKey);
		if (_in_scanKey == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_scanKey, _len_scanKey, _tmp_scanKey, _len_scanKey)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

		_in_scanKey[_len_scanKey - 1] = '\0';
		if (_len_scanKey != strlen(_in_scanKey) + 1)
		{
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

	ms->ms_retval = getTuple((const char*)_in_scanKey);
err:
	if (_in_scanKey) free(_in_scanKey);

	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv;} ecall_table[3];
} g_ecall_table = {
	3,
	{
		{(void*)(uintptr_t)sgx_initSOE, 0},
		{(void*)(uintptr_t)sgx_insert, 0},
		{(void*)(uintptr_t)sgx_getTuple, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[5][3];
} g_dyn_entry_table = {
	5,
	{
		{0, 0, 0, },
		{0, 0, 0, },
		{0, 0, 0, },
		{0, 0, 0, },
		{0, 0, 0, },
	}
};


sgx_status_t SGX_CDECL logger(const char* str)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = str ? strlen(str) + 1 : 0;

	ms_logger_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_logger_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(str, _len_str);

	ocalloc_size += (str != NULL) ? _len_str : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_logger_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_logger_t));
	ocalloc_size -= sizeof(ms_logger_t);

	if (str != NULL) {
		ms->ms_str = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, str, _len_str)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_str);
		ocalloc_size -= _len_str;
	} else {
		ms->ms_str = NULL;
	}
	
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileInit(const char* filename, unsigned int nblocks, unsigned int blocksize)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileInit_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileInit_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileInit_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileInit_t));
	ocalloc_size -= sizeof(ms_outFileInit_t);

	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	ms->ms_nblocks = nblocks;
	ms->ms_blocksize = blocksize;
	status = sgx_ocall(1, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileRead(char* block, const char* filename, int blkno)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_block = sizeof(char);
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileRead_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileRead_t);
	void *__tmp = NULL;

	void *__tmp_block = NULL;

	CHECK_ENCLAVE_POINTER(block, _len_block);
	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (block != NULL) ? _len_block : 0;
	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileRead_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileRead_t));
	ocalloc_size -= sizeof(ms_outFileRead_t);

	if (block != NULL) {
		ms->ms_block = (char*)__tmp;
		__tmp_block = __tmp;
		memset(__tmp_block, 0, _len_block);
		__tmp = (void *)((size_t)__tmp + _len_block);
		ocalloc_size -= _len_block;
	} else {
		ms->ms_block = NULL;
	}
	
	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	ms->ms_blkno = blkno;
	status = sgx_ocall(2, ms);

	if (status == SGX_SUCCESS) {
		if (block) {
			if (memcpy_s((void*)block, _len_block, __tmp_block, _len_block)) {
				sgx_ocfree();
				return SGX_ERROR_UNEXPECTED;
			}
		}
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileWrite(char* block, const char* filename, int oblkno)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_block = block ? strlen(block) + 1 : 0;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileWrite_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileWrite_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(block, _len_block);
	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (block != NULL) ? _len_block : 0;
	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileWrite_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileWrite_t));
	ocalloc_size -= sizeof(ms_outFileWrite_t);

	if (block != NULL) {
		ms->ms_block = (char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, block, _len_block)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_block);
		ocalloc_size -= _len_block;
	} else {
		ms->ms_block = NULL;
	}
	
	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	ms->ms_oblkno = oblkno;
	status = sgx_ocall(3, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileClose(const char* filename)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileClose_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileClose_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileClose_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileClose_t));
	ocalloc_size -= sizeof(ms_outFileClose_t);

	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	status = sgx_ocall(4, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

