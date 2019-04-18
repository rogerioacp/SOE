AUTOMAKE_OPTIONS = subdir-objects


#storage_SOURCES = backend/buffersgxbufpage.c
#storage_CFLAGS = 
#storage_LDADD = 
#enclave_SOURCES = backend/Enclave.edl

Sgx_common_flags = -m64 -O0 -g
Trts_Library_Name = sgx_trts
Crypto_Library_Name = sgx_tcrypto
Service_Library_Name = sgx_tservice

sgx_cflags = $(Sgx_common_flags) -nostdinc -fvisibility=hidden -fpie -ffunction-sections -fdata-sections -fstack-protector -I$(SGX_SDK_INCLUDE) -I$(SGX_SDK_INCLUDE)/tlibc -I$(SGX_SDK_INCLUDE)/libcxx






#sgx_addflags = -L$(SGX_SDK_LIB) -l$(Trts_Library_Name) -lsgx_tstdc -lsgx_tcxx -lsgx_tkey_exchange -l$(Crypto_Library_Name) -l$(Service_Library_Name)    

sgx_ldflags = $(sgx_common_flags) -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_SDK_LIB) \
	-Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -lsgx_tkey_exchange -l$(Crypto_Library_Name) -l$(Service_Library_Name) -Wl,--end-group \
	-Wl, -L/lib/x86_64-linux-gnu/ -Wl, -Bstatic -Wl,-Bsymbolic -Wl, --no-undefined \
	-Wl,-pie,-eenclave_entry -Wl, --export-dynamic  \
	-Wl,--defsym,__ImageBase=0 -Wl, --gc-sections   \
	-Wl,--version-script=backend/enclave/enclave.lds


if ENCLAVE_GEN
backend/enclave/Enclave_t.c: 
	$(SGX_EDGER8R) --trusted-dir backend/enclave --trusted Enclave.edl --search-path backend/enclave --search-path $(SGX_SDK_INCLUDE)
	mv backend/enclave/Enclave_t.h include/backend/enclave

backend/enclave/Enclave_u.c: 
	$(SGX_EDGER8R) --untrusted-dir backend/enclave --untrusted Enclave.edl --search-path backend/enclave --search-path $(SGX_SDK_INCLUDE)
	mv backend/enclave/Enclave_u.h include/backend/enclave
endif

#bin_PROGRAMS = enclavet

#enclavet_SOURCES = backend/enclave/Enclave_t.c backend/enclave/enclave.c main.c
#enclavet_CFLAGS = $(sgx_cflags) -I$(srcdir)/include/backend/enclave
#enclavet_LDFLAGS = $(sgx_ldflags)
#enclavet_LDADD = $(sgx_laddflags)

lib_LTLIBRARIES = libsoe.la
libsoe_la_SOURCES = backend/enclave/Enclave_t.c backend/enclave/enclave.c
libsoe_la_CFLAGS = $(sgx_cflags) -I$(srcdir)/include/backend/enclave
#libsoe_la_LIBADD = $(sgx_addflags)
libsoe_la_LDFLAGS = $(sgx_ldflags)

install-exec-hook:
	$(SGX_ENCLAVE_SIGNER) sign -key backend/enclave/private.pem -enclave $(DESTDIR)${libdir}/libsoe.so -out $(DESTDIR)${libdir}/libsoe.signed.so -config $(srcdir)/backend/enclave/Enclave.config.xml




